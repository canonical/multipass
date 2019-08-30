/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <src/platform/backends/qemu/qemu_virtual_machine_factory.h>

#include "mock_environment_helpers.h"
#include "mock_process_factory.h"
#include "mock_status_monitor.h"
#include "stub_process_factory.h"
#include "stub_ssh_key_provider.h"
#include "stub_status_monitor.h"
#include "temp_dir.h"
#include "temp_file.h"
#include "test_with_mocked_bin_path.h"

#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <QJsonArray>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{ // copied from QemuVirtualMachine implementation
constexpr auto suspend_tag = "suspend";
constexpr auto vm_command_version_tag = "command_version";

} // namespace

struct QemuBackend : public mpt::TestWithMockedBinPath
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mpt::TempDir data_dir;

    mpt::MockProcessFactory::Callback qemu_img_snapshot_returns_true = [](mpt::MockProcess* process) {
        // Have "qemu-img snapshot" return a string with the suspend tag in it
        if (process->program().contains("qemu-img") && process->arguments().contains("snapshot"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
            ON_CALL(*process, read_all_standard_output()).WillByDefault(Return(suspend_tag));
        }
    };
    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
};

TEST_F(QemuBackend, creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machine_start_shutdown_sends_monitoring_events)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::running;

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();
}

TEST_F(QemuBackend, machine_start_suspend_sends_monitoring_event)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::running;

    EXPECT_CALL(mock_monitor, on_suspend());
    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->suspend();
}

TEST_F(QemuBackend, throws_when_starting_while_suspending)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    machine->state = mp::VirtualMachine::State::suspending;

    EXPECT_THROW(machine->start(), std::runtime_error);
}

TEST_F(QemuBackend, machine_unknown_state_properly_shuts_down)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::unknown;

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, verify_dnsmasq_qemuimg_and_qemu_processes_created)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto factory = mpt::StubProcessFactory::Inject();
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    ASSERT_EQ(factory->process_list().size(), 3u);
    EXPECT_EQ(factory->process_list()[0].command, "dnsmasq");
    EXPECT_EQ(factory->process_list()[1].command, "qemu-img"); // checks for suspended image
    EXPECT_TRUE(factory->process_list()[2].command.startsWith("qemu-system-"));
}

TEST_F(QemuBackend, verify_some_common_qemu_arguments)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto factory = mpt::StubProcessFactory::Inject();
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    ASSERT_EQ(factory->process_list().size(), 3u);
    auto qemu = factory->process_list()[2];
    EXPECT_TRUE(qemu.arguments.contains("--enable-kvm"));
    EXPECT_TRUE(qemu.arguments.contains("virtio-net-pci,netdev=hostnet0,id=net0,mac="));
    EXPECT_TRUE(qemu.arguments.contains("-nographic"));
    EXPECT_TRUE(qemu.arguments.contains("-serial"));
    EXPECT_TRUE(qemu.arguments.contains("-qmp"));
    EXPECT_TRUE(qemu.arguments.contains("stdio"));
    EXPECT_TRUE(qemu.arguments.contains("-cpu"));
    EXPECT_TRUE(qemu.arguments.contains("host"));
    EXPECT_TRUE(qemu.arguments.contains("-chardev"));
    EXPECT_TRUE(qemu.arguments.contains("null,id=char0"));
}

TEST_F(QemuBackend, verify_qemu_arguments_when_resuming_suspend_image)
{
    constexpr auto default_machine_type = "pc-i440fx-xenial";

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(qemu_img_snapshot_returns_true);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    ASSERT_EQ(factory->process_list().size(), 3u);
    auto qemu = factory->process_list()[2];
    ASSERT_TRUE(qemu.command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu.arguments.contains("-loadvm"));
    EXPECT_TRUE(qemu.arguments.contains(suspend_tag));
    EXPECT_TRUE(qemu.arguments.contains("-machine"));
    EXPECT_TRUE(qemu.arguments.contains(default_machine_type));
}

TEST_F(QemuBackend, verify_qemu_arguments_when_resuming_suspend_image_uses_metadata)
{
    constexpr auto machine_type = "k0mPuT0R";

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(qemu_img_snapshot_returns_true);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_)).WillOnce(Return(QJsonObject({{"machine_type", machine_type}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    ASSERT_EQ(factory->process_list().size(), 3u);
    auto qemu = factory->process_list()[2];
    ASSERT_TRUE(qemu.command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu.arguments.contains("-machine"));
    EXPECT_TRUE(qemu.arguments.contains(machine_type));
}

TEST_F(QemuBackend, verify_qemu_command_version_when_resuming_suspend_image_using_cdrom_key)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(qemu_img_snapshot_returns_true);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_)).WillOnce(Return(QJsonObject({{"use_cdrom", true}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    ASSERT_EQ(factory->process_list().size(), 3u);
    auto qemu = factory->process_list()[2];
    ASSERT_TRUE(qemu.command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu.arguments.contains("-cdrom"));
}

TEST_F(QemuBackend, verify_qemu_arguments_from_metadata_are_used)
{
    constexpr auto suspend_tag = "suspend";

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-img") && process->arguments().contains("snapshot"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(suspend_tag));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(callback);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_))
        .WillOnce(Return(QJsonObject({{"arguments", QJsonArray{"-hi_there", "-hows_it_going"}}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    ASSERT_EQ(factory->process_list().size(), 3u);
    auto qemu = factory->process_list()[2];
    ASSERT_TRUE(qemu.command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu.arguments.contains("-hi_there"));
    EXPECT_TRUE(qemu.arguments.contains("-hows_it_going"));
}

TEST_F(QemuBackend, returns_version_string)
{
    constexpr auto qemu_version_output = "QEMU emulator version 2.11.1(Debian 1:2.11+dfsg-1ubuntu7.15)\n"
                                         "Copyright (c) 2003-2017 Fabrice Bellard and the QEMU Project developers\n";

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") && process->arguments().contains("--version"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(qemu_version_output));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(callback);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "qemu-2.11.1");
}

TEST_F(QemuBackend, returns_version_string_when_failed_parsing)
{
    constexpr auto qemu_version_output = "Unparsable version string";

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") && process->arguments().contains("--version"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output()).WillRepeatedly(Return(qemu_version_output));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(callback);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "qemu-unknown");
}

TEST_F(QemuBackend, returns_version_string_when_errored)
{
    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") && process->arguments().contains("--version"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 1;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return("Standard output\n"));
            EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return("Standard error\n"));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(callback);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "qemu-unknown");
}

TEST_F(QemuBackend, returns_version_string_when_exec_failed)
{
    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") && process->arguments().contains("--version"))
        {
            mp::ProcessState exit_state;
            exit_state.error = mp::ProcessState::Error{QProcess::Crashed, "Error message"};
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output()).Times(0);
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(callback);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "qemu-unknown");
}
