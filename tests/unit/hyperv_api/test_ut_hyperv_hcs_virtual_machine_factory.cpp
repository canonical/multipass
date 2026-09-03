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

#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hcs_ownership.h>
#include <hyperv_api/hcs_virtual_machine_exceptions.h>
#include <hyperv_api/hcs_virtual_machine_factory.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#include <multipass/network_interface.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_image.h>

#include "tests/unit/common.h"
#include "tests/unit/hyperv_api/mock_hyperv_hcn_wrapper.h"
#include "tests/unit/hyperv_api/mock_hyperv_hcs_wrapper.h"
#include "tests/unit/hyperv_api/mock_hyperv_virtdisk_wrapper.h"
#include "tests/unit/hyperv_api/mock_net_io_api.h"
#include "tests/unit/mock_platform.h"
#include "tests/unit/stub_availability_zone_manager.h"
#include "tests/unit/stub_ssh_key_provider.h"
#include "tests/unit/stub_status_monitor.h"
#include "tests/unit/temp_dir.h"

#include <fstream>
#include <memory>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;

using hcs_handle_t = mhv::hcs::HcsSystemHandle;
using hcs_op_result_t = mhv::OperationResult;
using uut_t = mhv::HCSVirtualMachineFactory;
using namespace testing;

struct HyperVHCSVirtualMachineFactory_UnitTests : public ::testing::Test
{
    mpt::TempDir dummy_data_dir;
    mpt::StubSSHKeyProvider stub_key_provider{};
    mpt::StubVMStatusMonitor stub_monitor{};
    mpt::StubAvailabilityZoneManager az_manager{};

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
        mpt::MockNetIOAPI::inject<NiceMock>();
    mpt::MockNetIOAPI& mock_net_io_api = *mock_net_io_api_injection.first;

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    inline static auto mock_handle_raw = reinterpret_cast<void*>(0xbadf00d);
    hcs_handle_t mock_handle{mock_handle_raw, [](void*) {}};

    auto construct_factory()
    {
        EXPECT_CALL(mock_hcn,
                    create_network(Field(&mhv::hcn::CreateNetworkParameters::name,
                                         Eq("Multipass vNetwork (zone1)"))))
            .WillOnce(DoAll(
                [&](const mhv::hcn::CreateNetworkParameters& params) {
                    EXPECT_EQ(params.type, mhv::hcn::HcnNetworkType::Ics());
                    EXPECT_EQ(params.guid,
                              multipass::utils::make_uuid("Multipass vNetwork (zone1)"));
                    EXPECT_EQ(params.policies.size(), 0);
                    ASSERT_EQ(params.ipams.size(), 1);
                    EXPECT_EQ(params.ipams[0].type, mhv::hcn::HcnIpamType::Static());
                    ASSERT_EQ(params.ipams[0].subnets.size(), 1);
                    EXPECT_EQ(params.ipams[0].subnets[0].ip_address_prefix,
                              az_manager.get_zone("zone1").get_subnet().to_cidr());
                },
                Return(hcs_op_result_t{0, L""})));

        return std::make_shared<uut_t>(dummy_data_dir.path(), az_manager);
    }
};

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, usesDedicatedBackendDirectory)
{
    const auto factory = construct_factory();

    EXPECT_EQ(factory->get_backend_directory_name(), "hyperv_api");
    EXPECT_EQ(QDir::cleanPath(factory->get_instance_directory("test-vm")),
              QDir::cleanPath(dummy_data_dir.filePath("hyperv_api/vault/instances/test-vm")));
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, remove_resources_for_impl_vm_exists)
{
    auto vm_name = "test-vm";
    auto vm_guid = "this isn't a guid but this isn't a real implementation either";
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll([&](const std::string& name, hcs_handle_t&) { ASSERT_EQ(vm_name, name); },
                        SetArgReferee<1>(mock_handle),
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, get_compute_system_guid(Eq(mock_handle), IsEmpty()))
        .WillOnce(DoAll(SetArgReferee<1>(vm_guid), Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(mock_hcn, enumerate_attached_endpoints(Eq(vm_guid), IsEmpty()))
        .WillOnce(DoAll(
            [&](const std::string& vm_guid, std::vector<std::string>& endpoint_guids) {
                endpoint_guids.emplace_back("this isn't an endpoint guid");
                endpoint_guids.emplace_back("this isn't either");
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcn, delete_endpoint(Eq("this isn't an endpoint guid")))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(mock_hcn, delete_endpoint(Eq("this isn't either")))
        .WillOnce(Return(hcs_op_result_t{0, L""}));
    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_factory());
    uut->remove_resources_for(vm_name);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, remove_resources_for_impl_does_not_exists)
{
    auto vm_name = "test-vm";
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll([&](const std::string& name, hcs_handle_t&) { ASSERT_EQ(vm_name, name); },
                        SetArgReferee<1>(mock_handle),
                        Return(hcs_op_result_t{1, L""})));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_factory());
    uut->remove_resources_for(vm_name);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests,
       release_resources_removes_deterministic_endpoint_without_compute_system)
{
    const std::string vm_name{"test-vm"};
    const std::string mac{"52:54:00:12:34:56"};
    EXPECT_CALL(mock_hcs, open_compute_system(vm_name, _))
        .WillOnce(Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_FOUND, L""}));
    EXPECT_CALL(mock_hcn, delete_endpoint(mhv::endpoint_guid_for_mac(mac)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_TRUE(mhv::release_hcs_resources(vm_name, {mac}));
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests,
       release_resources_keeps_endpoint_when_compute_system_cleanup_fails)
{
    const std::string vm_name{"test-vm"};
    const std::string mac{"52:54:00:12:34:56"};
    EXPECT_CALL(mock_hcs, open_compute_system(vm_name, _))
        .WillOnce(Return(hcs_op_result_t{E_FAIL, L""}));
    EXPECT_CALL(mock_hcn, delete_endpoint).Times(0);

    EXPECT_FALSE(mhv::release_hcs_resources(vm_name, {mac}));
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, prepare_instance_image)
{
    std::shared_ptr<uut_t> uut{nullptr};

    multipass::VirtualMachineDescription desc;
    desc.vm_name = "test-vm";
    desc.disk_space = multipass::MemorySize::from_bytes(123456);

    ASSERT_NO_THROW(uut = construct_factory());

    multipass::VMImage img;
    img.image_path = MP_PLATFORM.qstr_to_path(uut->get_instance_directory(desc.vm_name)) / "abcdef";
    EXPECT_CALL(mock_virtdisk,
                resize_virtual_disk(Eq(img.image_path), Eq(desc.disk_space.in_bytes())))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    uut->prepare_instance_image(img, desc);

    const auto ownership = mhv::HCSOwnership::load(
        MP_PLATFORM.qstr_to_path(uut->get_instance_directory(desc.vm_name)));
    ASSERT_TRUE(ownership);
    EXPECT_EQ(ownership->active_disk, img.image_path);
    EXPECT_EQ(ownership->state_file_stem, img.image_path);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, prepare_instance_image_failed)
{
    std::shared_ptr<uut_t> uut{nullptr};

    multipass::VMImage img;
    img.image_path = "abcdef";
    multipass::VirtualMachineDescription desc;
    desc.disk_space = multipass::MemorySize::from_bytes(123456);

    EXPECT_CALL(mock_virtdisk,
                resize_virtual_disk(Eq(img.image_path), Eq(desc.disk_space.in_bytes())))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    ASSERT_NO_THROW(uut = construct_factory());
    EXPECT_THROW(uut->prepare_instance_image(img, desc), mhv::ImageResizeException);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, create_virtual_machine)
{
    std::shared_ptr<uut_t> uut{nullptr};
    multipass::VirtualMachineDescription desc;
    desc.zone = "zone1";
    multipass::NetworkInterfaceInfo interface1{.id = "aabb", .type = "Ethernet"},
        interface2{.id = "bbaa", .type = "Ethernet"};

    desc.zone = "zone1";
    desc.extra_interfaces = {{.id = interface1.id}, {.id = interface2.id}};

    EXPECT_CALL(*mock_platform, get_network_interfaces_info())
        .WillRepeatedly(Return(
            std::map<std::string, multipass::NetworkInterfaceInfo>{{interface1.id, interface1},
                                                                   {interface2.id, interface2}}));

    EXPECT_CALL(mock_hcn, enumerate_networks)
        .WillOnce(DoAll(
            [&](std::vector<std::string>& network_guids) {
                // vSwitch for one already exists, one will be created from scratch
                network_guids.emplace_back("this isn't a network guid");
                // network_guids.emplace_back("this isn't either");
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcn, query_network(Eq("this isn't a network guid"), _))
        .WillOnce(DoAll(
            [&](const std::string&, mhv::hcn::HcnNetworkInfo& out) {
                out.guid = "this isn't a network guid";
                out.name = fmt::format("Multipass vSwitch ({})", interface1.id);
                out.type = "ICS";
                out.network_adapter_name = interface1.id;
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcn, create_network(_))
        // only expect call for bbaa. aabb's vSwitch already exists.
        .WillOnce(DoAll(
            [&](const mhv::hcn::CreateNetworkParameters& params) {
                constexpr auto expected_name = "Multipass vSwitch (bbaa)";
                EXPECT_EQ(params.name, expected_name);
                EXPECT_EQ(params.type, mhv::hcn::HcnNetworkType::Transparent());
                EXPECT_EQ(params.guid, multipass::utils::make_uuid(expected_name));
                ASSERT_EQ(params.policies.size(), 1);
                EXPECT_EQ(params.policies[0].type,
                          mhv::hcn::HcnNetworkPolicyType::NetAdapterName());
                ASSERT_TRUE(std::holds_alternative<mhv::hcn::HcnNetworkPolicyNetAdapterName>(
                    params.policies[0].settings));
                const auto& net_adapter_name = std::get<mhv::hcn::HcnNetworkPolicyNetAdapterName>(
                    params.policies[0].settings);
                EXPECT_EQ(net_adapter_name.net_adapter_name, interface2.id);
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(mock_handle), Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
        .WillRepeatedly(DoAll(
            [this](const hcs_handle_t& target_hcs_system,
                   void* context,
                   void (*callback)(HCS_EVENT* hcs_event, void* context)) {

            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(mhv::hcs::ComputeSystemState::running),
                              Return(hcs_op_result_t{0, L""})));

    ASSERT_NO_THROW(uut = construct_factory());

    uut->prepare_networking(desc.extra_interfaces);
    auto ptr = uut->create_virtual_machine(desc, stub_key_provider, stub_monitor);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, create_virtual_machine_uses_hcs_ownership)
{
    // A committed migration target records its own active disk and HCS state-file stem in an
    // ownership marker. create() must load that marker and boot the recorded disk/state, not
    // the disk path carried by the incoming description.
    const auto uut = construct_factory();

    const std::filesystem::path instance_dir{uut->get_instance_directory("test-vm").toStdString()};
    std::filesystem::create_directories(instance_dir);

    const auto migrated_disk = instance_dir / "migrated.vhdx";
    const auto state_stem = instance_dir / "hcs-migrated-state";
    const auto cloud_init = instance_dir / "cloud-init-config.iso";
    const auto description_disk = instance_dir / "unused-description.vhdx";
    for (const auto& file : {migrated_disk, cloud_init, description_disk})
        std::ofstream{file} << "stub";

    const mhv::HCSOwnership ownership{.active_disk = migrated_disk, .state_file_stem = state_stem};
    ownership.persist(instance_dir);

    multipass::VirtualMachineDescription desc;
    desc.vm_name = "test-vm";
    desc.zone = "zone1";
    desc.num_cores = 2;
    desc.mem_size = mp::MemorySize{"3M"};
    desc.default_mac_address = "aa:bb:cc:dd:ee:ff";
    desc.image.image_path = description_disk.string(); // deliberately NOT the ownership disk
    desc.cloud_init_iso = QString::fromStdString(cloud_init.string());

    // Force the create-from-scratch path so the disk/state actually flow into the request.
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillRepeatedly(Return(hcs_op_result_t{HCS_E_SYSTEM_NOT_FOUND, L""}));
    EXPECT_CALL(mock_hcs, set_compute_system_callback(_, _, _))
        .WillRepeatedly(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, get_compute_system_state(_, _))
        .WillRepeatedly(DoAll(SetArgReferee<1>(mhv::hcs::ComputeSystemState::running),
                              Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcn, delete_endpoint(EndsWith("aabbccddeeff")))
        .WillRepeatedly(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcn, create_endpoint(_)).WillRepeatedly(Return(hcs_op_result_t{0, L""}));

    // The chain must be resolved for the ownership disk, never the description disk.
    EXPECT_CALL(mock_virtdisk, list_virtual_disk_chain(Eq(migrated_disk), _, _))
        .WillRepeatedly(DoAll([&](const std::filesystem::path& disk,
                                  std::vector<std::filesystem::path>& chain,
                                  std::optional<std::size_t>) { chain.push_back(disk); },
                              Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, grant_vm_access(Eq("test-vm"), Eq(migrated_disk)))
        .WillRepeatedly(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, grant_vm_access(Eq("test-vm"), Eq(instance_dir)))
        .WillRepeatedly(Return(hcs_op_result_t{0, L""}));
    EXPECT_CALL(mock_hcs, grant_vm_access(Eq("test-vm"), Eq(cloud_init)))
        .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [&](const mhv::hcs::CreateComputeSystemParameters& params, hcs_handle_t&) {
                ASSERT_FALSE(params.scsi_devices.empty());
                EXPECT_EQ(params.scsi_devices.front().path.get(), migrated_disk);
                ASSERT_TRUE(params.guest_state.guest_state_file_path.has_value());
                EXPECT_THAT(params.guest_state.guest_state_file_path->get().string(),
                            StartsWith(state_stem.string()));
            },
            SetArgReferee<1>(mock_handle),
            Return(hcs_op_result_t{0, L""})));
    EXPECT_CALL(mock_hcs, start_compute_system(_)).WillRepeatedly(Return(hcs_op_result_t{0, L""}));

    auto vm = uut->create_virtual_machine(desc, stub_key_provider, stub_monitor);
    ASSERT_NE(vm, nullptr);
    vm->start();
}
