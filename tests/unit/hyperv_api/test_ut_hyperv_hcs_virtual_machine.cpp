/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcs_virtual_machine.h>
#include <hyperv_api/hcs_virtual_machine_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_snapshot.h>
#include <multipass/constants.h>
#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/mount_handler.h>
#include <multipass/utils.h>
#include <multipass/vm_mount.h>

#include "tests/unit/common.h"
#include "tests/unit/hyperv_api/mock_hyperv_hcn_wrapper.h"
#include "tests/unit/hyperv_api/mock_hyperv_hcs_wrapper.h"
#include "tests/unit/hyperv_api/mock_hyperv_virtdisk_wrapper.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_status_monitor.h"
#include "tests/unit/stub_availability_zone.h"
#include "tests/unit/stub_ssh_key_provider.h"
#include "tests/unit/stub_status_monitor.h"
#include "tests/unit/temp_dir.h"
#include "tests/unit/temp_file.h"
#include "tests/unit/windows/mock_net_io_api.h"
#include "tests/unit/windows/neighbor_table.h"

#include <algorithm>
#include <array>
#include <fstream>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace mhv = multipass::hyperv;
using uut_t = mhv::HCSVirtualMachine;
using hcs_handle_t = mhv::hcs::HcsSystemHandle;
using hcs_op_result_t = mhv::OperationResult;
using hcs_system_state_t = mhv::hcs::ComputeSystemState;
using native_path_t = multipass::NativePath;

struct PartiallyMockedHCSVM : public uut_t
{
    using uut_t::uut_t;

    MOCK_METHOD(std::string, ssh_exec, (const std::string& cmd, bool whisper), (override));
    MOCK_METHOD(void, drop_ssh_session, (), (override));
    MOCK_METHOD(void,
                add_extra_interface_to_instance_cloud_init,
                (const std::string&, const multipass::NetworkInterface&),
                (const, override));
};

using partially_mocked_uut_t = PartiallyMockedHCSVM;

struct HyperVHCSVirtualMachine_UnitTests : public ::testing::Test
{
    mpt::TempFile dummy_cloud_init_iso;
    mpt::TempDir dummy_instances_dir;
    const std::string dummy_vm_name{"lord-of-the-pings"};
    mpt::StubAvailabilityZone dummy_zone{};

    mp::VirtualMachineDescription desc{
        2,
        mp::MemorySize{"3M"},
        mp::MemorySize{}, // not used
        dummy_vm_name,
        dummy_zone.get_name(),
        "aa:bb:cc:dd:ee:ff",
        {},
        "",
        {dummy_instances_dir.filePath("base.vhdx").toStdString(), "", "", "", {}, {}},
        dummy_cloud_init_iso.name(),
        {},
        {},
        {},
        {}};

    mpt::StubSSHKeyProvider stub_key_provider{};
    mpt::StubVMStatusMonitor stub_monitor{};

    mpt::MockHCSWrapper::GuardedMock mock_hcs_wrapper_injection =
        mpt::MockHCSWrapper::inject<StrictMock>();
    mpt::MockHCSWrapper& mock_hcs = *mock_hcs_wrapper_injection.first;

    mpt::MockHCNWrapper::GuardedMock mock_hcn_wrapper_injection =
        mpt::MockHCNWrapper::inject<StrictMock>();
    mpt::MockHCNWrapper& mock_hcn = *mock_hcn_wrapper_injection.first;

    mpt::MockVirtDiskWrapper::GuardedMock mock_virtdisk_wrapper_injection =
        mpt::MockVirtDiskWrapper::inject<StrictMock>();
    mpt::MockVirtDiskWrapper& mock_virtdisk = *mock_virtdisk_wrapper_injection.first;

    mpt::MockNetIOAPI::GuardedMock mock_net_io_api_injection =
        mpt::MockNetIOAPI::inject<StrictMock>();
    mpt::MockNetIOAPI& mock_net_io_api = *mock_net_io_api_injection.first;

    // Host vNIC of the primary network ("abcd"); neighbor entries on other interfaces are ignored.
    static constexpr ULONG64 host_interface_luid = 42;
    static constexpr ULONG64 other_interface_luid = 7;

    static NET_LUID make_luid(ULONG64 value)
    {
        NET_LUID luid{};
        luid.Value = value;
        return luid;
    }

    inline static auto mock_handle_raw = reinterpret_cast<void*>(0xbadf00d);
    hcs_handle_t mock_handle{mock_handle_raw, [](void*) {}};
    void* compute_system_callback_context{nullptr};
    void (*compute_system_callback)(HCS_EVENT* hcs_event, void* context){nullptr};
    hcs_system_state_t api_state{hcs_system_state_t::running};

    hcs_op_result_t complete_system_exit()
    {
        api_state = hcs_system_state_t::stopped;

        HCS_EVENT event{};
        event.Type = HcsEventSystemExited;
        compute_system_callback(&event, compute_system_callback_context);

        return hcs_op_result_t{0, L""};
    }

    void SetUp() override
    {
        std::ofstream{desc.image.image_path} << "stub";

        ON_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
            .WillByDefault(DoAll(SaveArg<1>(&compute_system_callback_context),
                                 SaveArg<2>(&compute_system_callback),
                                 Return(hcs_op_result_t{0, L""})));
        EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
            .Times(AnyNumber());

        ON_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
            .WillByDefault([this](const hcs_handle_t&, hcs_system_state_t& out) {
                out = api_state;
                return hcs_op_result_t{0, L""};
            });
        EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _)).Times(AnyNumber());

        ON_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
            .WillByDefault([this](const hcs_handle_t&) {
                api_state = hcs_system_state_t::running;
                return hcs_op_result_t{0, L""};
            });
        ON_CALL(mock_hcs, resume_compute_system(Eq(mock_handle)))
            .WillByDefault([this](const hcs_handle_t&) {
                api_state = hcs_system_state_t::running;
                return hcs_op_result_t{0, L""};
            });

        ON_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
            .WillByDefault([this](const hcs_handle_t&) {
                api_state = hcs_system_state_t::paused;
                return hcs_op_result_t{0, L""};
            });
        EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle))).Times(AnyNumber());

        ON_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _))
            .WillByDefault(Return(hcs_op_result_t{0, L""}));
        EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _)).Times(AnyNumber());

        ON_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
            .WillByDefault([this](const hcs_handle_t&) { return complete_system_exit(); });
        EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle))).Times(AnyNumber());

        ON_CALL(mock_hcs, shutdown_compute_system(Eq(mock_handle)))
            .WillByDefault([this](const hcs_handle_t&) { return complete_system_exit(); });
        EXPECT_CALL(mock_hcs, shutdown_compute_system(Eq(mock_handle))).Times(AnyNumber());
        EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET)).Times(AnyNumber()).WillRepeatedly([] {
            return mpt::make_neighbor_table({});
        });

        EXPECT_CALL(mock_hcn, query_network(Eq("abcd"), _))
            .Times(AnyNumber())
            .WillRepeatedly([](const std::string&, mhv::hcn::HcnNetworkInfo& info) {
                info.host_interface_guid = "99F4AB49-031B-48CE-B1E3-69068EBEEA09";
                return hcs_op_result_t{0, L""};
            });
        EXPECT_CALL(mock_net_io_api, ConvertInterfaceGuidToLuid(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(
                DoAll(SetArgPointee<1>(make_luid(host_interface_luid)), Return(NO_ERROR)));
    }

    void default_open_success()
    {
        EXPECT_CALL(mock_hcs, open_compute_system(_, _))
            .WillRepeatedly(DoAll(
                [this](const std::string& name, hcs_handle_t&) { ASSERT_EQ(dummy_vm_name, name); },
                SetArgReferee<1>(mock_handle),
                Return(hcs_op_result_t{0, L""})));
    }

    void default_create_success()
    {
        // Open returns failure so the VM would be created
        EXPECT_CALL(mock_hcs, open_compute_system(_, _))
            .WillRepeatedly(
                DoAll([this](const std::string& name,
                             hcs_handle_t& out_handle) { ASSERT_EQ(dummy_vm_name, name); },
                      Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_FOUND, L""})));

        EXPECT_CALL(mock_hcn, delete_endpoint(EndsWith("aabbccddeeff")))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(mock_hcn, create_endpoint(_))
            .WillRepeatedly(DoAll(
                [this](const multipass::hyperv::hcn::CreateEndpointParameters& params) {
                    ASSERT_TRUE(params.mac_address.has_value());
                    ASSERT_EQ(params.mac_address.value(), "aa-bb-cc-dd-ee-ff");
                    ASSERT_EQ(params.network_guid, "abcd");
                },
                Return(hcs_op_result_t{0, L""})));

        EXPECT_CALL(mock_virtdisk, list_virtual_disk_chain(Eq(desc.image.image_path), _, _))
            .WillRepeatedly(
                DoAll([this](const native_path_t& vhdx_path,
                             std::vector<std::filesystem::path>& chain,
                             std::optional<std::size_t> max_depth) { chain.push_back(vhdx_path); },
                      Return(hcs_op_result_t{0, L""})));

        EXPECT_CALL(mock_hcs, grant_vm_access(Eq(dummy_vm_name), Eq(desc.image.image_path)))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(mock_hcs,
                    grant_vm_access(Eq(dummy_vm_name),
                                    Eq(native_path_t{dummy_instances_dir.path().toStdString()})))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(mock_hcs,
                    grant_vm_access(Eq(dummy_vm_name),
                                    Eq(native_path_t{dummy_cloud_init_iso.name().toStdString()})))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(mock_hcs, create_compute_system(_, _))
            .WillRepeatedly(DoAll(
                [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                       hcs_handle_t&) {
                    ASSERT_EQ(params.memory_size_mb, 3);
                    ASSERT_EQ(params.name, dummy_vm_name);
                    ASSERT_EQ(params.network_adapters.size(), 1);
                    ASSERT_EQ(params.processor_count, 2);
                    ASSERT_EQ(params.scsi_devices.size(), 2);
                    EXPECT_EQ(params.scsi_devices.front().path.get(), desc.image.image_path);
                },
                SetArgReferee<1>(mock_handle),
                Return(hcs_op_result_t{0, L""})));
    }

    // Makes HCN report the given networks, as (guid, name) pairs.
    void expect_networks(const std::vector<std::pair<std::string, std::string>>& networks)
    {
        std::vector<std::string> guids;
        for (const auto& [guid, name] : networks)
        {
            guids.push_back(guid);
            EXPECT_CALL(mock_hcn, query_network(Eq(guid), _))
                .WillRepeatedly([name](const std::string&, mhv::hcn::HcnNetworkInfo& info) {
                    info.name = name;
                    return hcs_op_result_t{0, L""};
                });
        }
        EXPECT_CALL(mock_hcn, enumerate_networks(_))
            .WillRepeatedly(DoAll(SetArgReferee<0>(guids), Return(hcs_op_result_t{0, L""})));
    }

    void expect_endpoint_query(std::vector<std::string> ip_addresses,
                               std::optional<std::string> mac_address = std::nullopt)
    {
        EXPECT_CALL(mock_hcn, query_endpoint(Eq("db4bdbf0-dc14-407f-9780-aabbccddeeff"), _))
            .WillOnce(
                [ip_addresses = std::move(ip_addresses),
                 mac_address = std::move(mac_address)](const std::string&,
                                                       mhv::hcn::HcnEndpointInfo& endpoint_info) {
                    endpoint_info.mac_address = mac_address;
                    endpoint_info.ip_addresses = ip_addresses;
                    return hcs_op_result_t{0, L""};
                });
    }

    void expect_endpoint_query_failure()
    {
        EXPECT_CALL(mock_hcn, query_endpoint(Eq("db4bdbf0-dc14-407f-9780-aabbccddeeff"), _))
            .WillOnce(Return(hcs_op_result_t{E_FAIL, L"Endpoint query failed"}));
    }

    void expect_failed_recreation()
    {
        EXPECT_CALL(mock_hcs, open_compute_system(dummy_vm_name, _))
            .WillRepeatedly(Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_FOUND, L""}));
        EXPECT_CALL(mock_hcn, delete_endpoint(EndsWith("aabbccddeeff")))
            .WillOnce(Return(hcs_op_result_t{0, L""}));
        EXPECT_CALL(mock_hcn, create_endpoint(_))
            .WillOnce(Return(hcs_op_result_t{E_FAIL, L"Endpoint creation failed"}));
        EXPECT_CALL(mock_hcs, create_compute_system(_, _)).Times(0);
    }

    void expect_permanent_neighbor(bool present, ULONG64 interface_luid = host_interface_luid)
    {
        EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET))
            .WillOnce(Return(
                ByMove(present ? mpt::make_neighbor_table({{{10, 123, 45, 67}, interface_luid}})
                               : mpt::make_neighbor_table({}))));
    }

    template <typename T = uut_t>
    std::shared_ptr<T> construct_vm(multipass::VMStatusMonitor* monitor = nullptr)
    {
        return std::make_shared<T>("abcd",
                                   desc,
                                   monitor ? *monitor : stub_monitor,
                                   stub_key_provider,
                                   dummy_zone,
                                   dummy_instances_dir.path());
    }
};

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_open)
{
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll(
            [this](const std::string& name, hcs_handle_t&) { ASSERT_EQ(dummy_vm_name, name); },
            SetArgReferee<1>(mock_handle),
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _)).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());
    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_open_error)
{
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll([this](const std::string& name,
                               hcs_handle_t& out_handle) { ASSERT_EQ(dummy_vm_name, name); },
                        Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_CONFIGURED_FOR_OPERATION, L""})));
    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_THROW(uut = construct_vm(), multipass::hyperv::OpenComputeSystemException);
    ASSERT_EQ(uut, nullptr);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_create)
{
    default_create_success();
    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, create_removes_stale_neighbors_for_management_mac)
{
    default_create_success();

    // The same MAC also has an entry on another network, which must be left alone.
    EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET)).WillOnce([] {
        return mpt::make_neighbor_table(
            {{{10, 97, 1, 82}, other_interface_luid}, {{10, 97, 0, 82}, host_interface_luid}});
    });
    EXPECT_CALL(mock_net_io_api, DeleteIpNetEntry2(_)).WillOnce([](const MIB_IPNET_ROW2* row) {
        const auto& address = row->Address.Ipv4.sin_addr.S_un.S_un_b;
        EXPECT_EQ(address.s_b1, 10);
        EXPECT_EQ(address.s_b2, 97);
        EXPECT_EQ(address.s_b3, 0);
        EXPECT_EQ(address.s_b4, 82);
        return NO_ERROR;
    });

    EXPECT_NO_THROW(construct_vm());
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, create_continues_when_stale_neighbors_cannot_be_queried)
{
    default_create_success();
    EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET)).WillOnce([] {
        auto table = mhv::IpNetTable{nullptr, [](MIB_IPNET_TABLE2*) {}};
        return mhv::IpNetTableResult{ERROR_ACCESS_DENIED, std::move(table)};
    });

    EXPECT_NO_THROW(construct_vm());
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, create_failure_removes_created_endpoint)
{
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_FOUND, L""}));
    EXPECT_CALL(mock_hcn, delete_endpoint(EndsWith("aabbccddeeff")))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L"not found"}))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcn, create_endpoint(_)).WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L"create failed"}));

    EXPECT_THROW(construct_vm(), mhv::CreateComputeSystemException);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, create_attaches_extra_interface_to_network_found_by_name)
{
    default_create_success();
    desc.extra_interfaces = {{"mpclitestsw0", "52:54:00:4d:50:01", false}};

    // A vSwitch created through Hyper-V has a GUID unrelated to its name.
    expect_networks({{"guid-default-switch", "Default Switch"}, {"guid-private", "mpclitestsw0"}});
    EXPECT_CALL(mock_hcn, delete_endpoint(EndsWith("5254004d5001")))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L"not found"}));
    EXPECT_CALL(mock_hcn,
                create_endpoint(
                    Field(&mhv::hcn::CreateEndpointParameters::network_guid, Eq("guid-private"))))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillOnce(DoAll([](const mhv::hcs::CreateComputeSystemParameters& params,
                           hcs_handle_t&) { EXPECT_EQ(params.network_adapters.size(), 2); },
                        SetArgReferee<1>(mock_handle),
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_NO_THROW(construct_vm());
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, create_attaches_extra_interface_to_multipass_network)
{
    default_create_success();
    desc.extra_interfaces = {{"Multipass vSwitch (Ethernet)", "52:54:00:4d:50:01", false}};

    // Networks created by Multipass keep a GUID derived from their name.
    const auto network_guid = mp::utils::make_uuid("Multipass vSwitch (Ethernet)");
    expect_networks({{network_guid, "Multipass vSwitch (Ethernet)"}});
    EXPECT_CALL(mock_hcn, delete_endpoint(EndsWith("5254004d5001")))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L"not found"}));
    EXPECT_CALL(
        mock_hcn,
        create_endpoint(Field(&mhv::hcn::CreateEndpointParameters::network_guid, Eq(network_guid))))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(mock_handle), Return(hcs_op_result_t{0, L""})));

    EXPECT_NO_THROW(construct_vm());
}

struct HyperVHCSVirtualMachine_UnresolvedNetwork
    : public HyperVHCSVirtualMachine_UnitTests,
      public WithParamInterface<std::vector<std::pair<std::string, std::string>>>
{
};

TEST_P(HyperVHCSVirtualMachine_UnresolvedNetwork, create_fails_before_creating_endpoints)
{
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillRepeatedly(Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_FOUND, L""}));
    desc.extra_interfaces = {{"mpclitestsw0", "52:54:00:4d:50:01", false}};
    expect_networks(GetParam());
    EXPECT_CALL(mock_hcn, create_endpoint(_)).Times(0);
    EXPECT_CALL(mock_hcs, create_compute_system(_, _)).Times(0);

    MP_EXPECT_THROW_THAT(construct_vm(),
                         mhv::CreateEndpointException,
                         mpt::match_what(HasSubstr("Could not find network `mpclitestsw0`")));
}

INSTANTIATE_TEST_SUITE_P(
    HyperVHCSVirtualMachine_UnitTests,
    HyperVHCSVirtualMachine_UnresolvedNetwork,
    Values(
        // No network with that name
        std::vector<std::pair<std::string, std::string>>{{"guid-default-switch", "Default Switch"}},
        // Ambiguous name
        std::vector<std::pair<std::string, std::string>>{{"guid-a", "mpclitestsw0"},
                                                         {"guid-b", "mpclitestsw0"}}));

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_start_success)
{
    default_open_success();
    api_state = hcs_system_state_t::stopped;

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);

    uut->start();

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::starting);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, existing_vm_cold_start_removes_stale_neighbors)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    auto uut = construct_vm();

    expect_permanent_neighbor(true);
    EXPECT_CALL(mock_net_io_api, DeleteIpNetEntry2(_)).WillOnce(Return(NO_ERROR));

    EXPECT_NO_THROW(uut->start());
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, saved_vm_start_keeps_permanent_neighbors)
{
    default_open_success();
    auto saved_state_path = std::filesystem::path{desc.image.image_path}.replace_extension(
        ".SavedState.vmrs");
    std::ofstream{saved_state_path} << "stub";

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    auto uut = construct_vm();

    EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET)).Times(0);

    EXPECT_NO_THROW(uut->start());
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, failed_saved_vm_recreation_remains_suspended)
{
    default_open_success();
    const auto saved_state_path = std::filesystem::path{desc.image.image_path}.replace_extension(
        ".SavedState.vmrs");
    std::ofstream{saved_state_path} << "stub";
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
    auto uut = construct_vm();
    ASSERT_EQ(uut->current_state(), mp::VirtualMachine::State::suspended);

    expect_failed_recreation();
    EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET)).Times(0);

    EXPECT_THROW(uut->start(), mhv::CreateEndpointException);
    EXPECT_EQ(uut->current_state(), mp::VirtualMachine::State::suspended);
    EXPECT_TRUE(std::filesystem::exists(saved_state_path));
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, failed_cold_vm_recreation_reports_off)
{
    default_open_success();
    auto uut = construct_vm();
    expect_failed_recreation();

    EXPECT_THROW(uut->start(), mhv::CreateEndpointException);
    EXPECT_EQ(uut->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, failure_after_compute_system_creation_drops_handle)
{
    default_open_success();
    auto uut = construct_vm();

    default_create_success();
    EXPECT_CALL(mock_virtdisk, list_virtual_disk_chain(Eq(desc.image.image_path), _, _))
        .WillOnce(Throw(std::runtime_error{"disk chain failed"}));

    EXPECT_THROW(uut->start(), std::runtime_error);

    // The terminated handle would still report `stopped`; reopening finds no compute system.
    EXPECT_EQ(uut->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, compute_system_open_error_reports_unknown_state)
{
    default_open_success();
    auto uut = construct_vm();
    expect_failed_recreation();
    EXPECT_THROW(uut->start(), mhv::CreateEndpointException);

    // Once for the explicit query, once more from the destructor's state check.
    EXPECT_CALL(mock_hcs, open_compute_system(dummy_vm_name, _))
        .Times(2)
        .WillRepeatedly(Return(hcs_op_result_t{E_ACCESSDENIED, L"Access denied"}));

    mp::VirtualMachine::State state{};
    EXPECT_NO_THROW(state = uut->current_state());
    EXPECT_EQ(state, mp::VirtualMachine::State::unknown);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, missing_compute_system_preserves_unavailable_state)
{
    default_open_success();
    auto uut = construct_vm();
    expect_failed_recreation();
    EXPECT_THROW(uut->start(), mhv::CreateEndpointException);
    uut->state = mp::VirtualMachine::State::unavailable;

    EXPECT_EQ(uut->current_state(), mp::VirtualMachine::State::unavailable);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_start_failure)
{
    default_open_success();
    api_state = hcs_system_state_t::stopped;

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);

    EXPECT_THROW(uut->start(), mhv::StartComputeSystemException);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_start_resume_success)
{
    default_open_success();
    api_state = hcs_system_state_t::paused;

    EXPECT_CALL(mock_hcs, resume_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::suspended);

    uut->start();

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::starting);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_start_resume_failure)
{
    default_open_success();
    api_state = hcs_system_state_t::paused;

    EXPECT_CALL(mock_hcs, resume_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::suspended);

    EXPECT_THROW(uut->start(), mhv::StartComputeSystemException);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::suspended);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_shutdown_success)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, shutdown_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Powerdown);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_shutdown_powerdown_fail)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, shutdown_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    std::shared_ptr<partially_mocked_uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm<partially_mocked_uut_t>());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    EXPECT_CALL(*uut, ssh_exec(Eq("sudo shutdown -h now"), _))
        .WillOnce([this](const std::string&, bool) {
            complete_system_exit();
            return std::string{};
        });
    EXPECT_CALL(*uut, drop_ssh_session()).Times(1);

    uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Powerdown);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_shutdown_halt)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<partially_mocked_uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm<partially_mocked_uut_t>());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    EXPECT_CALL(*uut, drop_ssh_session()).Times(1);

    uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Halt);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_shutdown_poweroff_suspended_removes_saved_state)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject<StrictMock>();
    default_open_success();

    bool state_file_removed = false;

    auto is_suspend_state_file = [](const std::filesystem::path& p) {
        return p.string().find(".SavedState.vmrs") != std::string::npos;
    };

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly([&](const mhv::hcs::HcsSystemHandle&, hcs_system_state_t& state) {
            state = hcs_system_state_t::stopped;
            return hcs_op_result_t{0, L""};
        });

    EXPECT_CALL(*mock_file_ops, exists(::testing::A<const std::filesystem::path&>()))
        .WillRepeatedly([&](const std::filesystem::path& p) { return !state_file_removed; });

    EXPECT_CALL(*mock_file_ops, remove(::testing::A<const std::filesystem::path&>(), _))
        .WillOnce([&](const std::filesystem::path& p, std::error_code& err) {
            if (is_suspend_state_file(p))
            {
                state_file_removed = true;
            }
            return true;
        });

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    ASSERT_EQ(uut->state, multipass::VirtualMachine::State::suspended);

    uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Poweroff);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
    EXPECT_EQ(state_file_removed, true);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests,
       vm_shutdown_poweroff_suspended_saved_state_removal_failure_throws)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject<StrictMock>();
    default_open_success();

    bool state_file_removal_attempted = false;

    auto is_suspend_state_file = [](const std::filesystem::path& p) {
        return p.string().find(".SavedState.vmrs") != std::string::npos;
    };

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly([&](const mhv::hcs::HcsSystemHandle&, hcs_system_state_t& state) {
            state = hcs_system_state_t::stopped;
            return hcs_op_result_t{0, L""};
        });

    EXPECT_CALL(*mock_file_ops, exists(::testing::A<const std::filesystem::path&>()))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_file_ops, remove(::testing::A<const std::filesystem::path&>(), _))
        .WillOnce([&](const std::filesystem::path& p, std::error_code& err) {
            if (is_suspend_state_file(p))
            {
                state_file_removal_attempted = true;
                err = std::make_error_code(std::errc::permission_denied);
            }
            return false;
        });

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    ASSERT_EQ(uut->state, multipass::VirtualMachine::State::suspended);

    EXPECT_THROW(uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Poweroff),
                 mhv::ShutdownComputeSystemException);
    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::suspended);
    EXPECT_EQ(state_file_removal_attempted, true);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_shutdown_termination_failure_throws)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L"termination failed"}));

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());
    EXPECT_THROW(uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Poweroff),
                 mhv::ShutdownComputeSystemException);

    // Change the state to stopped to prevent auto suspension
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_success)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    default_open_success();

    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle))).Times(1);
    EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _)).Times(1);
    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle))).Times(1);

    EXPECT_CALL(*mock_file_ops, exists(A<const std::filesystem::path&>()))
        .WillRepeatedly(Return(true));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    uut->suspend();

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::suspended);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_save_failure_resumes_and_throws)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .Times(2)
        .WillRepeatedly(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _))
        .WillOnce(Return(hcs_op_result_t{1, L"save failed"}));
    EXPECT_CALL(mock_hcs, resume_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_THROW(uut->suspend(), mhv::SaveComputeSystemException);

    // Change the state to stopped to prevent auto suspension
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_save_and_resume_failure_terminates_and_throws)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _))
        .WillOnce(Return(hcs_op_result_t{1, L"save failed"}));
    EXPECT_CALL(mock_hcs, resume_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L"resume failed"}));
    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_THROW(uut->suspend(), mhv::SaveComputeSystemException);
    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);

    // Change the state to stopped to prevent auto suspension
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_save_resume_and_terminate_failure_throws)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _))
        .WillOnce(Return(hcs_op_result_t{1, L"save failed"}));
    EXPECT_CALL(mock_hcs, resume_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L"resume failed"}));
    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L"termination failed"}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_THROW(uut->suspend(), mhv::SaveComputeSystemException);

    // Change the state to stopped to prevent auto suspension
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_termination_failure_throws)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L"termination failed"}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_THROW(uut->suspend(), mhv::ShutdownComputeSystemException);

    // Change the state to stopped to prevent auto suspension
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_on_destruction_persists_running_state)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    default_open_success();

    const auto saved_state_file = std::filesystem::path{desc.image.image_path}.replace_extension(
        ".SavedState.vmrs");
    EXPECT_CALL(*mock_file_ops, exists(TypedEq<const std::filesystem::path&>(saved_state_file)))
        .WillRepeatedly(Return(true));

    StrictMock<mpt::MockVMStatusMonitor> monitor;
    InSequence sequence;
    EXPECT_CALL(monitor, persist_state_for(dummy_vm_name, mp::VirtualMachine::State::running));
    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle))).Times(1);
    EXPECT_CALL(
        mock_hcs,
        save_compute_system(Eq(mock_handle), Property(&mp::NativePath::get, saved_state_file)))
        .Times(1);
    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle))).Times(1);
    EXPECT_CALL(monitor, persist_state_for(dummy_vm_name, mp::VirtualMachine::State::off));
    EXPECT_CALL(monitor, persist_state_for(dummy_vm_name, mp::VirtualMachine::State::suspended));
    EXPECT_CALL(monitor, persist_state_for(dummy_vm_name, mp::VirtualMachine::State::running));

    {
        auto uut = construct_vm(&monitor);
        EXPECT_EQ(uut->state, mp::VirtualMachine::State::running);
    }

    EXPECT_EQ(api_state, hcs_system_state_t::stopped);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_destruction_leaves_suspended_vm_suspended)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    default_open_success();
    api_state = hcs_system_state_t::stopped;

    EXPECT_CALL(*mock_file_ops, exists(A<const std::filesystem::path&>()))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle))).Times(0);
    EXPECT_CALL(mock_hcs, save_compute_system(Eq(mock_handle), _)).Times(0);
    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle))).Times(0);

    StrictMock<mpt::MockVMStatusMonitor> monitor;
    EXPECT_CALL(monitor, persist_state_for(dummy_vm_name, mp::VirtualMachine::State::suspended))
        .Times(1);

    {
        auto uut = construct_vm(&monitor);
        EXPECT_EQ(uut->state, mp::VirtualMachine::State::suspended);
    }

    EXPECT_EQ(api_state, hcs_system_state_t::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_set_unavailable)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    uut->set_available(false);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::unavailable);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_failure)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L""}))
        .RetiresOnSaturation();

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    ASSERT_THROW(uut->suspend(), multipass::hyperv::SaveComputeSystemException);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_ssh_port)
{
    default_open_success();

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());
    EXPECT_EQ(uut->ssh_port(), multipass::default_ssh_port);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_ssh_hostname)
{
    default_open_success();
    expect_endpoint_query({"10.123.45.67"});

    auto uut = construct_vm();

    EXPECT_EQ(uut->ssh_hostname(), "10.123.45.67");
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_ssh_hostname_throws_when_ip_is_unavailable)
{
    default_open_success();
    expect_endpoint_query_failure();

    auto uut = construct_vm();

    EXPECT_THROW((void)uut->ssh_hostname(), mp::IPUnavailableException);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_queries_primary_endpoint)
{
    default_open_success();
    expect_endpoint_query({"fe80::1", "1:2:3:4:5:6:7:8", "10.123.45.67"});

    auto uut = construct_vm();

    EXPECT_EQ(uut->management_ipv4(), mp::IPAddress{"10.123.45.67"});
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_queries_each_time)
{
    default_open_success();

    EXPECT_CALL(mock_hcn, query_endpoint(Eq("db4bdbf0-dc14-407f-9780-aabbccddeeff"), _))
        .WillOnce(DoAll(
            [](const std::string&, mhv::hcn::HcnEndpointInfo& endpoint_info) {
                endpoint_info.ip_addresses = {"10.123.45.67"};
            },
            Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll(
            [](const std::string&, mhv::hcn::HcnEndpointInfo& endpoint_info) {
                endpoint_info.ip_addresses = {"10.123.45.68"};
            },
            Return(hcs_op_result_t{0, L""})));

    auto uut = construct_vm();

    EXPECT_EQ(uut->management_ipv4(), mp::IPAddress{"10.123.45.67"});
    EXPECT_EQ(uut->management_ipv4(), mp::IPAddress{"10.123.45.68"});
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_retries_unsuccessful_query)
{
    default_open_success();

    EXPECT_CALL(mock_hcn, query_endpoint(Eq("db4bdbf0-dc14-407f-9780-aabbccddeeff"), _))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L"Endpoint query failed"}))
        .WillOnce(DoAll(
            [](const std::string&, mhv::hcn::HcnEndpointInfo& endpoint_info) {
                endpoint_info.ip_addresses = {"10.123.45.67"};
            },
            Return(hcs_op_result_t{0, L""})));

    auto uut = construct_vm();

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
    EXPECT_EQ(uut->management_ipv4(), mp::IPAddress{"10.123.45.67"});
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_returns_empty_without_ipv4_configuration)
{
    default_open_success();
    expect_endpoint_query({"fe80::1"});

    auto uut = construct_vm();

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_uses_permanent_neighbor)
{
    default_open_success();
    expect_endpoint_query({}, "aa-bb-cc-dd-ee-ff");

    auto uut = construct_vm();
    expect_permanent_neighbor(true);

    EXPECT_EQ(uut->management_ipv4(), mp::IPAddress{"10.123.45.67"});
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_returns_empty_without_neighbor)
{
    default_open_success();
    expect_endpoint_query({}, "aa-bb-cc-dd-ee-ff");

    auto uut = construct_vm();
    expect_permanent_neighbor(false);

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_is_quiet_without_endpoint)
{
    default_open_success();
    auto uut = construct_vm();

    // E.g. after restoring a snapshot, until the next start.
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(mock_hcn, query_endpoint(Eq("db4bdbf0-dc14-407f-9780-aabbccddeeff"), _))
        .WillOnce(Return(hcs_op_result_t{HCN_E_ENDPOINT_NOT_FOUND, L"not found"}));

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_logs_other_query_failures)
{
    default_open_success();
    auto uut = construct_vm();

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error, "failed to query endpoint");
    expect_endpoint_query_failure();

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_ignores_neighbor_on_other_interface)
{
    default_open_success();
    expect_endpoint_query({}, "aa-bb-cc-dd-ee-ff");

    auto uut = construct_vm();
    expect_permanent_neighbor(true, other_interface_luid);

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
}

TEST_F(HyperVHCSVirtualMachine_UnitTests, management_ipv4_returns_empty_without_host_interface)
{
    default_open_success();
    expect_endpoint_query({}, "aa-bb-cc-dd-ee-ff");

    auto uut = construct_vm();
    EXPECT_CALL(mock_hcn, query_network(Eq("abcd"), _))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L"Network query failed"}));
    EXPECT_CALL(mock_net_io_api, GetIpNetTable2(AF_INET)).Times(0);

    EXPECT_EQ(uut->management_ipv4(), std::nullopt);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, update_state_calls_persist_state_only_on_change)
{
    default_open_success();
    api_state = hcs_system_state_t::paused;

    mpt::MockVMStatusMonitor mock_monitor{};
    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})))
        .WillOnce(
            DoAll(SetArgReferee<1>(hcs_system_state_t::running), Return(hcs_op_result_t{0, L""})))
        .WillRepeatedly(
            DoAll(SetArgReferee<1>(hcs_system_state_t::stopped), Return(hcs_op_result_t{0, L""})));

    InSequence sequence;
    EXPECT_CALL(
        mock_monitor,
        persist_state_for(Eq(dummy_vm_name), Eq(multipass::VirtualMachine::State::running)));
    EXPECT_CALL(
        mock_monitor,
        persist_state_for(Eq(dummy_vm_name), Eq(multipass::VirtualMachine::State::stopped)));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm(&mock_monitor));

    EXPECT_EQ(uut->current_state(), multipass::VirtualMachine::State::running);
    EXPECT_EQ(uut->current_state(), multipass::VirtualMachine::State::running);
    EXPECT_EQ(uut->current_state(), multipass::VirtualMachine::State::stopped);
    EXPECT_EQ(uut->current_state(), multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, update_cpus)
{
    default_create_success();
    api_state = hcs_system_state_t::stopped;

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    uut->update_cpus(55);

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t&) {
                ASSERT_EQ(params.memory_size_mb, 3);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 55);
                ASSERT_EQ(params.scsi_devices.size(), 2);
            },
            SetArgReferee<1>(mock_handle),
            Return(hcs_op_result_t{0, L""})));
    uut->start();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, resize_memory)
{
    default_create_success();
    api_state = hcs_system_state_t::stopped;

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    uut->resize_memory(multipass::MemorySize::from_bytes((1024ll * 1024 * 1024) * 10));

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t&) {
                ASSERT_EQ(params.memory_size_mb, 10240);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 2);
                ASSERT_EQ(params.scsi_devices.size(), 2);
            },
            SetArgReferee<1>(mock_handle),
            Return(hcs_op_result_t{0, L""})));
    uut->start();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, resize_disk)
{
    default_create_success();
    api_state = hcs_system_state_t::stopped;

    EXPECT_CALL(mock_virtdisk, resize_virtual_disk(Eq(desc.image.image_path), Eq(123456)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle))).Times(1);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    mp::UserMessages messages{};
    uut->resize_disk(multipass::MemorySize::from_bytes(123456), messages);

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t&) {
                ASSERT_EQ(params.memory_size_mb, 3);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 2);
                ASSERT_EQ(params.scsi_devices.size(), 2);
            },
            SetArgReferee<1>(mock_handle),
            Return(hcs_op_result_t{0, L""})));
    uut->start();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, add_network_interface)
{
    default_open_success();
    api_state = hcs_system_state_t::stopped;

    multipass::NetworkInterface if_to_add;
    if_to_add.mac_address = "ff:ee:dd:cc:bb:aa";
    if_to_add.id = "floaterface";

    std::shared_ptr<partially_mocked_uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm<partially_mocked_uut_t>());

    std::string endpoint_guid{};

    EXPECT_CALL(*uut, add_extra_interface_to_instance_cloud_init(_, _)).Times(1);
    uut->add_network_interface(0, "ff:ee:dd:cc:bb:aa", if_to_add);
}
