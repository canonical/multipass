/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include "mock_qemu_platform.h"

#include "tests/common.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_status_monitor.h"
#include "tests/stub_process_factory.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"
#include "tests/temp_file.h"
#include "tests/test_with_mocked_bin_path.h"

#include <src/platform/backends/qemu/qemu_virtual_machine.h>
#include <src/platform/backends/qemu/qemu_virtual_machine_factory.h>

#include <multipass/auto_join_thread.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{ // copied from QemuVirtualMachine implementation
constexpr auto suspend_tag = "suspend";
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
                                                      {},
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", {}, {}},
                                                      dummy_cloud_init_iso.name(),
                                                      {},
                                                      {},
                                                      {},
                                                      {}};
    mpt::TempDir data_dir;
    const std::string tap_device{"tapfoo"};
    const QString bridge_name{"dummy-bridge"};
    const std::string subnet{"192.168.64"};

    mpt::MockProcessFactory::Callback handle_external_process_calls = [](mpt::MockProcess* process) {
        // Have "qemu-img snapshot" return a string with the suspend tag in it
        if (process->program().contains("qemu-img") && process->arguments().contains("snapshot"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
            ON_CALL(*process, read_all_standard_output()).WillByDefault(Return(suspend_tag));
        }
        else if (process->program() == "iptables")
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
        }
    };

    static void handle_qemu_system(mpt::MockProcess* process)
    {
        if (process->program().contains("qemu-system"))
        {
            EXPECT_CALL(*process, start()).WillRepeatedly([process] {
                emit process->state_changed(QProcess::Running);
                emit process->started();
            });

            EXPECT_CALL(*process, wait_for_finished(_)).WillRepeatedly(Return(true));

            EXPECT_CALL(*process, write(_)).WillRepeatedly([process](const QByteArray& data) {
                QJsonParseError parse_error;
                auto json = QJsonDocument::fromJson(data, &parse_error);
                if (parse_error.error == QJsonParseError::NoError)
                {
                    auto json_object = json.object();
                    auto execute = json_object["execute"];

                    if (execute == "system_powerdown")
                    {
                        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce([process](auto...) {
                            mp::ProcessState exit_state{0, mp::nullopt};
                            emit process->finished(exit_state);
                            return true;
                        });
                    }
                    else if (execute == "human-monitor-command")
                    {
                        auto args = json_object["arguments"].toObject();
                        auto command_line = args["command-line"];
                        if (command_line == "savevm suspend")
                        {
                            EXPECT_CALL(*process, read_all_standard_output())
                                .WillRepeatedly(Return(
                                    "{\"timestamp\": {\"seconds\": 1541188919, \"microseconds\": 838498}, \"event\": "
                                    "\"RESUME\"}"));

                            EXPECT_CALL(*process, kill()).WillOnce([process] {
                                mp::ProcessState exit_state{
                                    mp::nullopt, mp::ProcessState::Error{QProcess::Crashed, QStringLiteral("")}};
                                emit process->error_occurred(QProcess::Crashed, "Crashed");
                                emit process->finished(exit_state);
                            });
                            emit process->ready_read_standard_output();
                        }
                    }
                }

                return data.size();
            });

            if (process->arguments().contains("-dump-vmstate"))
            {
                mp::ProcessState exit_state;
                exit_state.exit_code = 0;
                EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            }
        }
    };

    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    std::unique_ptr<mpt::MockProcessFactory::Scope> process_factory{mpt::MockProcessFactory::Inject()};
};

TEST_F(QemuBackend, creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machine_in_off_state_handles_shutdown)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));

    machine->shutdown();
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machine_start_shutdown_sends_monitoring_events)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    process_factory->register_callback(handle_qemu_system);

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

    process_factory->register_callback(handle_qemu_system);

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

TEST_F(QemuBackend, throws_when_shutdown_while_starting)
{
    mpt::MockProcess* vmproc = nullptr;
    process_factory->register_callback([&vmproc](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains("-dump-vmstate")) // we only care about the actual vm process
        {
            vmproc = process; // save this to control later
        }
    });

    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    machine->start();
    ASSERT_EQ(machine->state, mp::VirtualMachine::State::starting);

    mp::AutoJoinThread thread{[&machine, vmproc] {
        ON_CALL(*vmproc, running()).WillByDefault(Return(false));
        machine->shutdown();
    }};

    using namespace std::chrono_literals;
    while (machine->state != mp::VirtualMachine::State::off)
        std::this_thread::sleep_for(1ms);

    MP_EXPECT_THROW_THAT(machine->ensure_vm_is_running(), mp::StartException,
                         Property(&mp::StartException::name, Eq(machine->vm_name)));
    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(QemuBackend, includes_error_when_shutdown_while_starting)
{
    constexpr auto error_msg = "failing spectacularly";
    mpt::MockProcess* vmproc = nullptr;
    process_factory->register_callback([&vmproc](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains("-dump-vmstate")) // we only care about the actual vm process
        {
            vmproc = process; // save this to control later
            EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return(error_msg));
        }
    });

    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    machine->start(); // we need this so that Process signals get connected to their handlers
    EXPECT_EQ(machine->state, mp::VirtualMachine::State::starting);

    ASSERT_TRUE(vmproc);
    emit vmproc->ready_read_standard_error();                 // fake process standard error having something to read
    ON_CALL(*vmproc, running()).WillByDefault(Return(false)); /* simulate process not running anymore,
                                                                 to avoid blocking on destruction */

    mp::AutoJoinThread finishing_thread{[vmproc]() {
        mp::ProcessState exit_state;
        exit_state.exit_code = 1;
        emit vmproc->finished(exit_state); /* note that this waits on a condition variable that is unblocked by
                                              ensure_vm_is_running */
    }};

    using namespace std::chrono_literals;
    while (machine->state != mp::VirtualMachine::State::off)
        std::this_thread::sleep_for(1ms);

    MP_EXPECT_THROW_THAT(
        machine->ensure_vm_is_running(), mp::StartException,
        AllOf(Property(&mp::StartException::name, Eq(machine->vm_name)),
              mpt::match_what(AllOf(HasSubstr(error_msg), HasSubstr("shutdown"), HasSubstr("starting")))));
}

TEST_F(QemuBackend, machine_unknown_state_properly_shuts_down)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    process_factory->register_callback(handle_qemu_system);

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
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = factory->process_list();
    EXPECT_TRUE(std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::StubProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command == "qemu-img";
                             }) != processes.cend());

    EXPECT_TRUE(std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::StubProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             }) != processes.cend());
}

TEST_F(QemuBackend, verify_some_common_qemu_arguments)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    process_factory->register_callback(handle_external_process_calls);
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    EXPECT_TRUE(qemu->arguments.contains("--enable-kvm"));
    EXPECT_TRUE(qemu->arguments.contains("virtio-net-pci,netdev=hostnet0,id=net0,mac="));
    EXPECT_TRUE(qemu->arguments.contains("-nographic"));
    EXPECT_TRUE(qemu->arguments.contains("-serial"));
    EXPECT_TRUE(qemu->arguments.contains("-qmp"));
    EXPECT_TRUE(qemu->arguments.contains("stdio"));
    EXPECT_TRUE(qemu->arguments.contains("-cpu"));
    EXPECT_TRUE(qemu->arguments.contains("host"));
    EXPECT_TRUE(qemu->arguments.contains("-chardev"));
    EXPECT_TRUE(qemu->arguments.contains("null,id=char0"));
}

TEST_F(QemuBackend, verify_qemu_arguments_when_resuming_suspend_image)
{
    process_factory->register_callback(handle_external_process_calls);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    EXPECT_TRUE(qemu->arguments.contains("-loadvm"));
    EXPECT_TRUE(qemu->arguments.contains(suspend_tag));
}

TEST_F(QemuBackend, verify_qemu_arguments_when_resuming_suspend_image_uses_metadata)
{
    constexpr auto machine_type = "k0mPuT0R";

    process_factory->register_callback(handle_external_process_calls);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_)).WillOnce(Return(QJsonObject({{"machine_type", machine_type}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    ASSERT_TRUE(qemu->command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu->arguments.contains("-machine"));
    EXPECT_TRUE(qemu->arguments.contains(machine_type));
}

TEST_F(QemuBackend, verify_qemu_command_version_when_resuming_suspend_image_using_cdrom_key)
{
    process_factory->register_callback(handle_external_process_calls);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_)).WillOnce(Return(QJsonObject({{"use_cdrom", true}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    ASSERT_TRUE(qemu->command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu->arguments.contains("-cdrom"));
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

    process_factory->register_callback(callback);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_))
        .WillOnce(Return(QJsonObject({{"arguments", QJsonArray{"-hi_there", "-hows_it_going"}}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(), processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    EXPECT_TRUE(qemu->arguments.contains("-hi_there"));
    EXPECT_TRUE(qemu->arguments.contains("-hows_it_going"));
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

    process_factory->register_callback(callback);

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

    process_factory->register_callback(callback);

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

    process_factory->register_callback(callback);

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

    process_factory->register_callback(callback);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "qemu-unknown");
}

TEST_F(QemuBackend, ssh_hostname_returns_expected_value)
{
    mpt::StubVMStatusMonitor stub_monitor;
    const std::string expected_ip{"10.10.0.34"};
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    ON_CALL(mock_qemu_platform, get_ip_for(_))
        .WillByDefault([&expected_ip](auto...) { return mp::optional<mp::IPAddress>{expected_ip}; });

    mp::QemuVirtualMachine machine{default_description, &mock_qemu_platform, stub_monitor};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_EQ(machine.VirtualMachine::ssh_hostname(), expected_ip);
}

TEST_F(QemuBackend, gets_management_ip)
{
    mpt::StubVMStatusMonitor stub_monitor;
    const std::string expected_ip{"10.10.0.35"};
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    EXPECT_CALL(mock_qemu_platform, get_ip_for(_)).WillOnce(Return(expected_ip));

    mp::QemuVirtualMachine machine{default_description, &mock_qemu_platform, stub_monitor};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_EQ(machine.management_ipv4(), expected_ip);
}

TEST_F(QemuBackend, fails_to_get_management_ip_if_dnsmasq_does_not_return_an_ip)
{
    mpt::StubVMStatusMonitor stub_monitor;
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    EXPECT_CALL(mock_qemu_platform, get_ip_for(_)).WillOnce(Return(mp::nullopt));

    mp::QemuVirtualMachine machine{default_description, &mock_qemu_platform, stub_monitor};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_EQ(machine.management_ipv4(), "UNKNOWN");
}

TEST_F(QemuBackend, ssh_hostname_timeout_throws_and_sets_unknown_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    ON_CALL(mock_qemu_platform, get_ip_for(_)).WillByDefault([](auto...) { return mp::nullopt; });

    mp::QemuVirtualMachine machine{default_description, &mock_qemu_platform, stub_monitor};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_THROW(machine.ssh_hostname(std::chrono::milliseconds(1)), std::runtime_error);
    EXPECT_EQ(machine.state, mp::VirtualMachine::State::unknown);
}

TEST_F(QemuBackend, lists_no_networks)
{
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_THROW(backend.networks(), mp::NotImplementedOnThisBackendException);
}
