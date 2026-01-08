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
#include <multipass/constants.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/mount_handler.h>
#include <multipass/vm_mount.h>

#include "tests/common.h"
#include "tests/hyperv_api/mock_hyperv_hcn_wrapper.h"
#include "tests/hyperv_api/mock_hyperv_hcs_wrapper.h"
#include "tests/hyperv_api/mock_hyperv_virtdisk_wrapper.h"
#include "tests/mock_status_monitor.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"
#include "tests/temp_file.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace mhv = multipass::hyperv;
using uut_t = mhv::HCSVirtualMachine;
using hcs_handle_t = mhv::hcs::HcsSystemHandle;
using hcs_op_result_t = mhv::OperationResult;
using hcs_system_state_t = mhv::hcs::ComputeSystemState;

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
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::TempDir dummy_instances_dir;
    const std::string dummy_vm_name{"lord-of-the-pings"};

    mp::VirtualMachineDescription desc{2,
                                       mp::MemorySize{"3M"},
                                       mp::MemorySize{}, // not used
                                       dummy_vm_name,
                                       "aa:bb:cc:dd:ee:ff",
                                       {},
                                       "",
                                       {dummy_image.name(), "", "", "", {}, {}},
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

    inline static auto mock_handle_raw = reinterpret_cast<void*>(0xbadf00d);
    hcs_handle_t mock_handle{mock_handle_raw, [](void*) {}};
    void* compute_system_callback_context{nullptr};
    void (*compute_system_callback)(HCS_EVENT* hcs_event, void* context){nullptr};

    void default_open_success()
    {
        EXPECT_CALL(mock_hcs, open_compute_system(_, _))
            .WillRepeatedly(DoAll(
                [this](const std::string& name, hcs_handle_t& out_handle) {
                    ASSERT_EQ(dummy_vm_name, name);
                    out_handle = mock_handle;
                },
                Return(hcs_op_result_t{0, L""})));

        EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
            .WillRepeatedly(DoAll(
                [this](const hcs_handle_t& target_hcs_system,
                       void* context,
                       void (*callback)(HCS_EVENT* hcs_event, void* context)) {
                    compute_system_callback_context = context;
                    compute_system_callback = callback;
                },
                Return(hcs_op_result_t{0, L""})));

        EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
            .WillRepeatedly(
                DoAll([this](const hcs_handle_t&,
                             hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                      Return(hcs_op_result_t{0, L""})));
    }

    void default_create_success()
    {
        // Open returns failure so the VM would be created
        EXPECT_CALL(mock_hcs, open_compute_system(_, _))
            .WillRepeatedly(
                DoAll([this](const std::string& name,
                             hcs_handle_t& out_handle) { ASSERT_EQ(dummy_vm_name, name); },
                      Return(hcs_op_result_t{1, L""})));

        EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
            .WillRepeatedly(
                DoAll([this](const hcs_handle_t&,
                             hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                      Return(hcs_op_result_t{0, L""})));

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

        EXPECT_CALL(mock_virtdisk,
                    list_virtual_disk_chain(Eq(dummy_image.name().toStdString()), _, _))
            .WillRepeatedly(
                DoAll([this](const std::filesystem::path& vhdx_path,
                             std::vector<std::filesystem::path>& chain,
                             std::optional<std::size_t> max_depth) { chain.push_back(vhdx_path); },
                      Return(hcs_op_result_t{0, L""})));

        EXPECT_CALL(mock_hcs,
                    grant_vm_access(Eq(dummy_vm_name), Eq(dummy_image.name().toStdString())))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(
            mock_hcs,
            grant_vm_access(Eq(dummy_vm_name), Eq(dummy_cloud_init_iso.name().toStdString())))
            .WillRepeatedly(Return(hcs_op_result_t{0, L""}));

        EXPECT_CALL(mock_hcs, create_compute_system(_, _))
            .WillRepeatedly(DoAll(
                [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                       hcs_handle_t& out_hcs_system) {
                    ASSERT_EQ(params.memory_size_mb, 3);
                    ASSERT_EQ(params.name, dummy_vm_name);
                    ASSERT_EQ(params.network_adapters.size(), 1);
                    ASSERT_EQ(params.processor_count, 2);
                    ASSERT_EQ(params.scsi_devices.size(), 2);
                    ASSERT_EQ(params.shares.size(), 0);
                    out_hcs_system = mock_handle;
                },
                Return(hcs_op_result_t{0, L""})));
    }

    template <typename T = uut_t>
    std::shared_ptr<T> construct_vm(multipass::VMStatusMonitor* monitor = nullptr)
    {
        return std::make_shared<T>("abcd",
                                   desc,
                                   monitor ? *monitor : stub_monitor,
                                   stub_key_provider,
                                   dummy_instances_dir.path());
    }
};

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_open)
{
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll(
            [this](const std::string& name, hcs_handle_t& out_handle) {
                ASSERT_EQ(dummy_vm_name, name);
                out_handle = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .Times(1)
        .WillRepeatedly(
            DoAll([this](const hcs_handle_t&,
                         hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                  Return(hcs_op_result_t{0, L""})));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());
    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_create)
{
    default_create_success();
    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_start_success)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                        Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);

    uut->start();

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::starting);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_start_failure)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll([](const hcs_handle_t&,
                     hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                  Return(hcs_op_result_t{0, L""})));

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

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::paused; },
                        Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

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

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(DoAll([](const hcs_handle_t&,
                                 hcs_system_state_t& state) { state = hcs_system_state_t::paused; },
                              Return(hcs_op_result_t{0, L""})));

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

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, shutdown_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

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

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, shutdown_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    std::shared_ptr<partially_mocked_uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm<partially_mocked_uut_t>());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    EXPECT_CALL(*uut, ssh_exec(Eq("sudo shutdown -h now"), _)).Times(1);
    EXPECT_CALL(*uut, drop_ssh_session()).Times(1);

    uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Powerdown);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_shutdown_halt)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<partially_mocked_uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm<partially_mocked_uut_t>());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    EXPECT_CALL(*uut, drop_ssh_session()).Times(1);

    uut->shutdown(multipass::VirtualMachine::ShutdownPolicy::Halt);

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::stopped);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_success)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::paused; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    uut->suspend();

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::suspended);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, vm_suspend_failure)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll([](const hcs_handle_t&,
                     hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                  Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, pause_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    EXPECT_EQ(uut->state, multipass::VirtualMachine::State::running);

    uut->suspend();

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

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());
    EXPECT_EQ(uut->ssh_hostname({}), uut->get_name() + ".mshome.net");
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, update_state)
{
    default_open_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([](const hcs_handle_t&,
                           hcs_system_state_t& state) { state = hcs_system_state_t::paused; },
                        Return(hcs_op_result_t{0, L""})));

    mpt::MockVMStatusMonitor mock_monitor{};
    EXPECT_CALL(
        mock_monitor,
        persist_state_for(Eq(dummy_vm_name), Eq(multipass::VirtualMachine::State::suspended)))
        .Times(2);

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm(&mock_monitor));

    uut->handle_state_update();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, update_cpus)
{
    default_create_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll([](const hcs_handle_t&,
                     hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                  Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    uut->update_cpus(55);

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t& out_hcs_system) {
                ASSERT_EQ(params.memory_size_mb, 3);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 55);
                ASSERT_EQ(params.scsi_devices.size(), 2);
                ASSERT_EQ(params.shares.size(), 0);
                out_hcs_system = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));
    uut->start();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, resize_memory)
{
    default_create_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll([](const hcs_handle_t&,
                     hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                  Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    uut->resize_memory(multipass::MemorySize::from_bytes((1024ll * 1024 * 1024) * 10));

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t& out_hcs_system) {
                ASSERT_EQ(params.memory_size_mb, 10240);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 2);
                ASSERT_EQ(params.scsi_devices.size(), 2);
                ASSERT_EQ(params.shares.size(), 0);
                out_hcs_system = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));
    uut->start();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, resize_disk)
{
    default_create_success();

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll([](const hcs_handle_t&,
                     hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                  Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_virtdisk,
                resize_virtual_disk(Eq(desc.image.image_path.toStdString()), Eq(123456)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(mock_hcs, start_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm());

    uut->resize_disk(multipass::MemorySize::from_bytes(123456));

    EXPECT_CALL(mock_hcs, create_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t& out_hcs_system) {
                ASSERT_EQ(params.memory_size_mb, 3);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 2);
                ASSERT_EQ(params.scsi_devices.size(), 2);
                ASSERT_EQ(params.shares.size(), 0);
                out_hcs_system = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));
    uut->start();
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, add_network_interface)
{
    default_open_success();

    multipass::NetworkInterface if_to_add;
    if_to_add.mac_address = "ff:ee:dd:cc:bb:aa";
    if_to_add.id = "floaterface";

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(
            DoAll([](const hcs_handle_t&,
                     hcs_system_state_t& state) { state = hcs_system_state_t::stopped; },
                  Return(hcs_op_result_t{0, L""})));

    std::shared_ptr<partially_mocked_uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_vm<partially_mocked_uut_t>());

    std::string endpoint_guid{};

    EXPECT_CALL(*uut, add_extra_interface_to_instance_cloud_init(_, _)).Times(1);
    uut->add_network_interface(0, "ff:ee:dd:cc:bb:aa", if_to_add);
}
