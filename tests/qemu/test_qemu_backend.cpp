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

#include "mock_qemu_platform.h"

#include "tests/common.h"
#include "tests/mock_cloud_init_file_ops.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_logger.h"
#include "tests/mock_platform.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_snapshot.h"
#include "tests/mock_status_monitor.h"
#include "tests/mock_virtual_machine.h"
#include "tests/path.h"
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
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/snapshot.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <thread>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
// copied from QemuVirtualMachine implementation
constexpr auto suspend_tag = "suspend";
// we need a whitespace to terminate the tag column in the fake output of qemu-img
const QByteArray fake_snapshot_list_with_suspend_tag = QByteArray{suspend_tag} + " ";
} // namespace

struct QemuBackend : public mpt::TestWithMockedBinPath
{
    QemuBackend()
    {
        EXPECT_CALL(*mock_qemu_platform, remove_resources_for(_)).WillRepeatedly(Return());
        EXPECT_CALL(*mock_qemu_platform, vm_platform_args(_)).WillRepeatedly(Return(QStringList()));
        EXPECT_CALL(*mock_qemu_platform, get_directory_name()).WillRepeatedly(Return(QString()));
    };

    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      {},
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", {}, {}},
                                                      dummy_cloud_init_iso.name(),
                                                      {},
                                                      {},
                                                      {},
                                                      {}};
    mpt::TempDir data_dir;
    mpt::TempDir instance_dir;
    const std::string tap_device{"tapfoo"};
    const QString bridge_name{"dummy-bridge"};
    const std::string subnet{"192.168.64"};
    mpt::StubSSHKeyProvider key_provider{};
    mpt::StubVMStatusMonitor stub_monitor{};

    mpt::MockProcessFactory::Callback handle_external_process_calls =
        [](mpt::MockProcess* process) {
            // Have "qemu-img snapshot" return a string with the suspend tag in it
            if (process->program().contains("qemu-img") &&
                process->arguments().contains("snapshot"))
            {
                mp::ProcessState exit_state;
                exit_state.exit_code = 0;
                ON_CALL(*process, execute(_)).WillByDefault(Return(exit_state));
                ON_CALL(*process, read_all_standard_output())
                    .WillByDefault(Return(fake_snapshot_list_with_suspend_tag));
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
                            mp::ProcessState exit_state{0, std::nullopt};
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
                                .WillRepeatedly(Return("{\"timestamp\": {\"seconds\": 1541188919, "
                                                       "\"microseconds\": 838498}, \"event\": "
                                                       "\"RESUME\"}"));

                            EXPECT_CALL(*process, kill()).WillOnce([process] {
                                mp::ProcessState exit_state{
                                    std::nullopt,
                                    mp::ProcessState::Error{QProcess::Crashed, QStringLiteral("")}};
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

    mpt::MockLogger::Scope logger_scope{mpt::MockLogger::inject()};

    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    std::unique_ptr<mpt::MockProcessFactory::Scope> process_factory{
        mpt::MockProcessFactory::Inject()};

    std::unique_ptr<mpt::MockQemuPlatform> mock_qemu_platform{
        std::make_unique<mpt::MockQemuPlatform>()};

    mpt::MockQemuPlatformFactory::GuardedMock qemu_platform_factory_attr{
        mpt::MockQemuPlatformFactory::inject<NiceMock>()};
    mpt::MockQemuPlatformFactory* mock_qemu_platform_factory{qemu_platform_factory_attr.first};
};

TEST_F(QemuBackend, createsInOffState)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machineInOffStateHandlesShutdown)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));

    machine->shutdown();
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machineStartShutdownSendsMonitoringEvents)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    process_factory->register_callback(handle_qemu_system);

    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::running;

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();
}

TEST_F(QemuBackend, machineStartSuspendSendsMonitoringEvent)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    process_factory->register_callback(handle_qemu_system);

    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::running;

    EXPECT_CALL(mock_monitor, on_suspend());
    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->suspend();
}

TEST_F(QemuBackend, throwsWhenShutdownWhileStarting)
{
    mpt::MockProcess* vmproc = nullptr;
    process_factory->register_callback([&vmproc](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains(
                "-dump-vmstate")) // we only care about the actual vm process
        {
            vmproc = process; // save this to control later
        }
    });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->start();
    ASSERT_EQ(machine->state, mp::VirtualMachine::State::starting);

    mp::AutoJoinThread thread{[&machine, vmproc] {
        ON_CALL(*vmproc, running()).WillByDefault(Return(false));
        machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff);
    }};

    using namespace std::chrono_literals;
    while (machine->state != mp::VirtualMachine::State::off)
        std::this_thread::sleep_for(1ms);

    MP_EXPECT_THROW_THAT(machine->ensure_vm_is_running(),
                         mp::StartException,
                         Property(&mp::StartException::name, Eq(machine->vm_name)));
    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(QemuBackend, throwsOnShutdownTimeout)
{
    static const std::string sub_error_msg1{"The QEMU process did not finish within "};
    static const std::string sub_error_msg2{"seconds after being shutdown"};

    mpt::MockProcess* vmproc = nullptr;
    process_factory->register_callback([&vmproc](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains(
                "-dump-vmstate")) // we only care about the actual vm process
        {
            vmproc = process; // save this to control later
        }
    });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->start();

    ASSERT_TRUE(vmproc);
    EXPECT_CALL(*vmproc, wait_for_finished).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(*vmproc, running).WillOnce(Return(true)).WillRepeatedly(Return(false));

    machine->state = mp::VirtualMachine::State::running;

    MP_EXPECT_THROW_THAT(
        machine->shutdown(),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr(sub_error_msg1), HasSubstr(sub_error_msg2))));

    EXPECT_NE(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(QemuBackend, includesErrorWhenShutdownWhileStarting)
{
    constexpr auto error_msg = "failing spectacularly";
    mpt::MockProcess* vmproc = nullptr;
    process_factory->register_callback([&vmproc](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains(
                "-dump-vmstate")) // we only care about the actual vm process
        {
            vmproc = process; // save this to control later
            EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return(error_msg));
        }
    });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->start(); // we need this so that Process signals get connected to their handlers
    EXPECT_EQ(machine->state, mp::VirtualMachine::State::starting);

    ASSERT_TRUE(vmproc);
    emit vmproc
        ->ready_read_standard_error(); // fake process standard error having something to read
    ON_CALL(*vmproc, running())
        .WillByDefault(Return(false)); /* simulate process not running anymore,
                                          to avoid blocking on destruction */

    mp::AutoJoinThread finishing_thread{[vmproc]() {
        mp::ProcessState exit_state;
        exit_state.exit_code = 1;
        emit vmproc->finished(exit_state); /* note that this waits on a condition variable that is
                                              unblocked by ensure_vm_is_running */
    }};

    using namespace std::chrono_literals;
    while (machine->state != mp::VirtualMachine::State::off)
        std::this_thread::sleep_for(1ms);

    MP_EXPECT_THROW_THAT(
        machine->ensure_vm_is_running(),
        mp::StartException,
        AllOf(Property(&mp::StartException::name, Eq(machine->vm_name)),
              mpt::match_what(
                  AllOf(HasSubstr(error_msg), HasSubstr("shutdown"), HasSubstr("starting")))));
}

TEST_F(QemuBackend, machineUnknownStateProperlyShutsDown)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    process_factory->register_callback(handle_qemu_system);

    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::unknown;

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, suspendedStateNoForceShutdownThrows)
{
    const std::string sub_error_msg{"Cannot shut down suspended instance"};

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->state = mp::VirtualMachine::State::suspended;

    MP_EXPECT_THROW_THAT(machine->shutdown(),
                         mp::VMStateInvalidException,
                         mpt::match_what(HasSubstr(sub_error_msg)));

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::suspended);
}

TEST_F(QemuBackend, suspendingStateNoForceShutdownThrows)
{
    const std::string sub_error_msg1{"Cannot shut down instance"};
    const std::string sub_error_msg2{"while suspending."};

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->state = mp::VirtualMachine::State::suspending;

    MP_EXPECT_THROW_THAT(
        machine->shutdown(),
        mp::VMStateInvalidException,
        mpt::match_what(AllOf(HasSubstr(sub_error_msg1), HasSubstr(sub_error_msg2))));

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::suspending);
}

TEST_F(QemuBackend, startingStateNoForceShutdownThrows)
{
    const std::string sub_error_msg1{"Cannot shut down instance"};
    const std::string sub_error_msg2{"while starting."};

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->state = mp::VirtualMachine::State::starting;

    MP_EXPECT_THROW_THAT(
        machine->shutdown(),
        mp::VMStateInvalidException,
        mpt::match_what(AllOf(HasSubstr(sub_error_msg1), HasSubstr(sub_error_msg2))));

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::starting);
}

TEST_F(QemuBackend, forceShutdownKillsProcessAndLogs)
{
    mpt::MockProcess* vmproc = nullptr;
    process_factory->register_callback([&vmproc](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains(
                "-dump-vmstate")) // we only care about the actual vm process
        {
            vmproc = process; // save this to control later
            EXPECT_CALL(*process, kill()).WillOnce([process] {
                mp::ProcessState exit_state{
                    std::nullopt,
                    mp::ProcessState::Error{QProcess::Crashed, QStringLiteral("Force stopped")}};
                emit process->error_occurred(QProcess::Crashed, "Killed");
                emit process->finished(exit_state);
            });
        }
    });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "process program");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "process arguments");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "process started");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Forcing shutdown");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Killing process");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Killed");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Force stopped");

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->start(); // we need this so that Process signals get connected to their handlers

    ASSERT_TRUE(vmproc);

    machine->state = mp::VirtualMachine::State::running;

    machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff); // force shutdown

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(QemuBackend, forceShutdownNoProcessLogs)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Forcing shutdown");
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "No process to kill");

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->state = mp::VirtualMachine::State::unknown;

    machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff); // force shutdown

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(QemuBackend, forceShutdownSuspendDeletesSuspendImageAndOffState)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::MockProcessFactory::Callback snapshot_list_suspend_tag_callback =
        [](mpt::MockProcess* process) {
            if (process->program().contains("qemu-img") &&
                process->arguments().contains("snapshot") && process->arguments().contains("-l"))
            {
                EXPECT_CALL(*process, read_all_standard_output())
                    .WillOnce(Return(fake_snapshot_list_with_suspend_tag));
            }
        };
    process_factory->register_callback(snapshot_list_suspend_tag_callback);

    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Forcing shutdown");
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "No process to kill");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Deleting suspend image");

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    const auto machine =
        backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->state = mp::VirtualMachine::State::suspended;
    machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff);

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);

    const std::vector<mpt::MockProcessFactory::ProcessInfo> processes =
        process_factory->process_list();
    EXPECT_FALSE(processes.empty());
    EXPECT_TRUE(processes.back().command == "qemu-img" &&
                processes.back().arguments.contains("-d") &&
                processes.back().arguments.contains(suspend_tag));
}

TEST_F(QemuBackend, forceShutdownSuspendedStateButNoSuspensionSnapshotInImage)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Forcing shutdown");
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "No process to kill");
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         "Image has no suspension snapshot, but the state is 7");

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    const auto machine =
        backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->state = mp::VirtualMachine::State::suspended;
    machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff);

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(QemuBackend, forceShutdownRunningStateButWithSuspensionSnapshotInImage)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::MockProcessFactory::Callback snapshot_list_suspend_tag_callback =
        [](mpt::MockProcess* process) {
            if (process->program().contains("qemu-img") &&
                process->arguments().contains("snapshot") && process->arguments().contains("-l"))
            {
                EXPECT_CALL(*process, read_all_standard_output())
                    .WillOnce(Return(fake_snapshot_list_with_suspend_tag));
            }
        };
    process_factory->register_callback(snapshot_list_suspend_tag_callback);

    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Forcing shutdown");
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "No process to kill");
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Deleting suspend image");
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         "Image has a suspension snapshot, but the state is 4");

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    const auto machine =
        backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->state = mp::VirtualMachine::State::running;
    machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff);

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);

    const std::vector<mpt::MockProcessFactory::ProcessInfo> processes =
        process_factory->process_list();
    EXPECT_FALSE(processes.empty());
    EXPECT_TRUE(processes.back().command == "qemu-img" &&
                processes.back().arguments.contains("-d") &&
                processes.back().arguments.contains(suspend_tag));
}

TEST_F(QemuBackend, verifyDnsmasqQemuimgAndQemuProcessesCreated)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    auto factory = mpt::StubProcessFactory::Inject();
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = factory->process_list();
    EXPECT_TRUE(std::find_if(processes.cbegin(),
                             processes.cend(),
                             [](const mpt::StubProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command == "qemu-img";
                             }) != processes.cend());

    EXPECT_TRUE(std::find_if(processes.cbegin(),
                             processes.cend(),
                             [](const mpt::StubProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             }) != processes.cend());
}

TEST_F(QemuBackend, verifySomeCommonQemuArguments)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::MockProcess* qemu = nullptr;
    process_factory->register_callback([&qemu](mpt::MockProcess* process) {
        if (process->program().startsWith("qemu-system-") &&
            !process->arguments().contains(
                "-dump-vmstate")) // we only care about the actual vm process
        {
            qemu = process;
        }
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    ASSERT_TRUE(qemu != nullptr);

    const auto qemu_args = qemu->arguments();
    EXPECT_TRUE(qemu_args.contains("-nographic"));
    EXPECT_TRUE(qemu_args.contains("-serial"));
    EXPECT_TRUE(qemu_args.contains("-qmp"));
    EXPECT_TRUE(qemu_args.contains("stdio"));
    EXPECT_TRUE(qemu_args.contains("-chardev"));
    EXPECT_TRUE(qemu_args.contains("null,id=char0"));
}

TEST_F(QemuBackend, verifyQemuArgumentsWhenResumingSuspendImage)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    process_factory->register_callback(handle_external_process_calls);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(),
                             processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    EXPECT_TRUE(qemu->arguments.contains("-loadvm"));
    EXPECT_TRUE(qemu->arguments.contains(suspend_tag));
}

TEST_F(QemuBackend, verifyQemuArgumentsWhenResumingSuspendImageUsesMetadata)
{
    constexpr auto machine_type = "k0mPuT0R";

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    process_factory->register_callback(handle_external_process_calls);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_))
        .WillRepeatedly(Return(QJsonObject({{"machine_type", machine_type}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(),
                             processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    ASSERT_TRUE(qemu->command.startsWith("qemu-system-"));
    EXPECT_TRUE(qemu->arguments.contains("-machine"));
    EXPECT_TRUE(qemu->arguments.contains(machine_type));
}

TEST_F(QemuBackend, verifyQemuArgumentsFromMetadataAreUsed)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-img") && process->arguments().contains("snapshot"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output())
                .WillOnce(Return(fake_snapshot_list_with_suspend_tag));
        }
    };

    process_factory->register_callback(callback);
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    EXPECT_CALL(mock_monitor, retrieve_metadata_for(_))
        .WillRepeatedly(
            Return(QJsonObject({{"arguments", QJsonArray{"-hi_there", "-hows_it_going"}}})));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);
    machine->start();
    machine->state = mp::VirtualMachine::State::running;

    auto processes = process_factory->process_list();
    auto qemu = std::find_if(processes.cbegin(),
                             processes.cend(),
                             [](const mpt::MockProcessFactory::ProcessInfo& process_info) {
                                 return process_info.command.startsWith("qemu-system-");
                             });

    ASSERT_TRUE(qemu != processes.cend());
    EXPECT_TRUE(qemu->arguments.contains("-hi_there"));
    EXPECT_TRUE(qemu->arguments.contains("-hows_it_going"));
}

TEST_F(QemuBackend, returnsVersionString)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    constexpr auto qemu_version_output =
        "QEMU emulator version 2.11.1(Debian 1:2.11+dfsg-1ubuntu7.15)\n"
        "Copyright (c) 2003-2017 Fabrice Bellard and the QEMU Project developers\n";

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") &&
            process->arguments().contains("--version"))
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

TEST_F(QemuBackend, returnsVersionStringWhenFailedParsing)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    constexpr auto qemu_version_output = "Unparsable version string";

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") &&
            process->arguments().contains("--version"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_output())
                .WillRepeatedly(Return(qemu_version_output));
        }
    };

    process_factory->register_callback(callback);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "qemu-unknown");
}

TEST_F(QemuBackend, returnsVersionStringWhenErrored)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") &&
            process->arguments().contains("--version"))
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

TEST_F(QemuBackend, returnsVersionStringWhenExecFailed)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::MockProcessFactory::Callback callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-system-") &&
            process->arguments().contains("--version"))
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

TEST_F(QemuBackend, sshHostnameReturnsExpectedValue)
{
    const std::string expected_ip{"10.10.0.34"};
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    ON_CALL(mock_qemu_platform, get_ip_for(_)).WillByDefault([&expected_ip](auto...) {
        return std::optional<mp::IPAddress>{expected_ip};
    });

    mp::QemuVirtualMachine machine{default_description,
                                   &mock_qemu_platform,
                                   stub_monitor,
                                   key_provider,
                                   instance_dir.path()};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_EQ(machine.VirtualMachine::ssh_hostname(), expected_ip);
}

TEST_F(QemuBackend, getsManagementIp)
{
    const mp::IPAddress expected_ip{"10.10.0.35"};
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    EXPECT_CALL(mock_qemu_platform, get_ip_for(_)).WillOnce(Return(expected_ip));

    mp::QemuVirtualMachine machine{default_description,
                                   &mock_qemu_platform,
                                   stub_monitor,
                                   key_provider,
                                   instance_dir.path()};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_EQ(machine.management_ipv4(), expected_ip.as_string()); // TODO@ricab revert as_string
}

TEST_F(QemuBackend, failsToGetManagementIpIfDnsmasqDoesNotReturnAnIp)
{
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    EXPECT_CALL(mock_qemu_platform, get_ip_for(_)).WillOnce(Return(std::nullopt));

    mp::QemuVirtualMachine machine{default_description,
                                   &mock_qemu_platform,
                                   stub_monitor,
                                   key_provider,
                                   instance_dir.path()};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_EQ(machine.management_ipv4(), std::nullopt);
}

TEST_F(QemuBackend, sshHostnameTimeoutThrowsAndSetsUnknownState)
{
    NiceMock<mpt::MockQemuPlatform> mock_qemu_platform;

    ON_CALL(mock_qemu_platform, get_ip_for(_)).WillByDefault([](auto...) { return std::nullopt; });

    mp::QemuVirtualMachine machine{default_description,
                                   &mock_qemu_platform,
                                   stub_monitor,
                                   key_provider,
                                   instance_dir.path()};
    machine.start();
    machine.state = mp::VirtualMachine::State::running;

    EXPECT_THROW(machine.ssh_hostname(std::chrono::milliseconds(1)), std::runtime_error);
    EXPECT_EQ(machine.state, mp::VirtualMachine::State::unknown);
}

struct MockQemuVM : public mpt::MockVirtualMachineT<mp::QemuVirtualMachine>
{
    using mpt::MockVirtualMachineT<mp::QemuVirtualMachine>::MockVirtualMachineT;
    using mp::QemuVirtualMachine::make_specific_snapshot;

    MOCK_METHOD(void, drop_ssh_session, (), (override));
};

TEST_F(QemuBackend, dropsSSHSessionWhenStopping)
{
    NiceMock<MockQemuVM> machine{"mock-qemu-vm", key_provider};
    machine.state = multipass::VirtualMachine::State::running;

    EXPECT_CALL(machine, drop_ssh_session());

    MP_DELEGATE_MOCK_CALLS_ON_BASE(machine, shutdown, mp::QemuVirtualMachine);
    machine.shutdown(mp::VirtualMachine::ShutdownPolicy::Powerdown);
}

TEST_F(QemuBackend, supportsSnapshots)
{
    MockQemuVM vm{"asdf", key_provider};
}

TEST_F(QemuBackend, createsQemuSnapshotsFromSpecs)
{
    MockQemuVM machine{"mock-qemu-vm", key_provider};

    auto snapshot_name = "elvis";
    auto snapshot_comment = "has left the building";
    auto instance_id = "vm1";

    const mp::VMSpecs specs{2,
                            mp::MemorySize{"3.21G"},
                            mp::MemorySize{"4.32M"},
                            "00:00:00:00:00:00",
                            {{"eth18", "18:18:18:18:18:18", true}},
                            "asdf",
                            mp::VirtualMachine::State::stopped,
                            {},
                            false,
                            {}};
    auto snapshot = machine.make_specific_snapshot(snapshot_name,
                                                   snapshot_comment,
                                                   instance_id,
                                                   specs,
                                                   nullptr);
    EXPECT_EQ(snapshot->get_name(), snapshot_name);
    EXPECT_EQ(snapshot->get_comment(), snapshot_comment);
    EXPECT_EQ(snapshot->get_num_cores(), specs.num_cores);
    EXPECT_EQ(snapshot->get_mem_size(), specs.mem_size);
    EXPECT_EQ(snapshot->get_disk_space(), specs.disk_space);
    EXPECT_EQ(snapshot->get_extra_interfaces(), specs.extra_interfaces);
    EXPECT_EQ(snapshot->get_state(), specs.state);
    EXPECT_EQ(snapshot->get_parent(), nullptr);
}

TEST_F(QemuBackend, createsQemuSnapshotsFromJsonFile)
{
    MockQemuVM machine{"mock-qemu-vm", key_provider};

    const auto parent = std::make_shared<mpt::MockSnapshot>();
    EXPECT_CALL(machine, get_snapshot(2)).WillOnce(Return(parent));
    const mpt::MockCloudInitFileOps::GuardedMock mock_cloud_init_file_ops_injection =
        mpt::MockCloudInitFileOps::inject<NiceMock>();
    EXPECT_CALL(*mock_cloud_init_file_ops_injection.first, get_instance_id_from_cloud_init(_))
        .Times(1);
    auto snapshot = machine.make_specific_snapshot(mpt::test_data_path_for("test_snapshot.json"));
    EXPECT_EQ(snapshot->get_name(), "snapshot3");
    EXPECT_EQ(snapshot->get_comment(), "A comment");
    EXPECT_EQ(snapshot->get_num_cores(), 1);
    EXPECT_EQ(snapshot->get_mem_size(), mp::MemorySize{"1G"});
    EXPECT_EQ(snapshot->get_disk_space(), mp::MemorySize{"5G"});
    EXPECT_EQ(snapshot->get_extra_interfaces(), std::vector<mp::NetworkInterface>{});
    EXPECT_EQ(snapshot->get_state(), mp::VirtualMachine::State::off);
    EXPECT_EQ(snapshot->get_parent(), parent);
}

TEST_F(QemuBackend, networksReturnsSupportedNetworks)
{
    ON_CALL(*mock_qemu_platform, is_network_supported(_)).WillByDefault(Return(true));

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    const std::map<std::string, mp::NetworkInterfaceInfo> networks{
        {"mpbr0", {"mpbr0", "bridge", "gobbledygook"}},
        {"virbr0", {"virbr0", "bridge", "gobbledygook"}},
        {"mpqemubr0", {"mpqemubr0", "bridge", "gobbledygook"}},
        {"enxe4b97a832426", {"enxe4b97a832426", "ethernet", "gobbledygook"}}};

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).WillOnce(Return(networks));

    auto supported_nets = backend.networks();
    EXPECT_EQ(supported_nets.size(), networks.size());
}

TEST_F(QemuBackend, removeResourcesForCallsQemuPlatform)
{
    bool remove_resources_called{false};
    const std::string test_name{"foo"};

    EXPECT_CALL(*mock_qemu_platform, remove_resources_for(_))
        .WillOnce([&remove_resources_called, &test_name](auto& name) {
            remove_resources_called = true;

            EXPECT_EQ(name, test_name);
        });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    backend.remove_resources_for(test_name);

    EXPECT_TRUE(remove_resources_called);
}

TEST_F(QemuBackend, hypervisorHealthCheckCallsQemuPlatform)
{
    bool health_check_called{false};

    EXPECT_CALL(*mock_qemu_platform, platform_health_check()).WillOnce([&health_check_called] {
        health_check_called = true;
    });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    backend.hypervisor_health_check();

    EXPECT_TRUE(health_check_called);
}

TEST_F(QemuBackend, getBackendDirectoryNameCallsQemuPlatform)
{
    bool get_directory_name_called{false};
    const QString backend_dir_name{"foo"};

    EXPECT_CALL(*mock_qemu_platform, get_directory_name())
        .Times(2)
        .WillRepeatedly([&get_directory_name_called, &backend_dir_name] {
            get_directory_name_called = true;

            return backend_dir_name;
        });

    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    const auto dir_name = backend.get_backend_directory_name();

    EXPECT_EQ(dir_name, backend_dir_name);
    EXPECT_TRUE(get_directory_name_called);
}

TEST_F(QemuBackend, addNetworkInterface)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    const auto [mock_cloud_init_file_ops, _] = mpt::MockCloudInitFileOps::inject();
    EXPECT_CALL(*mock_cloud_init_file_ops, add_extra_interface_to_cloud_init).Times(1);

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    EXPECT_NO_THROW(machine->add_network_interface(0, "", {"", "", true}));
}

TEST_F(QemuBackend, createBridgeWithChecksWithQemuPlatform)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });
    EXPECT_CALL(*mock_qemu_platform, needs_network_prep()).Times(1).WillRepeatedly(Return(true));

    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    std::vector<mp::NetworkInterface> extra_interfaces{{"eth1", "52:54:00:00:00:00", true}};
    EXPECT_NO_THROW(backend.prepare_networking(extra_interfaces));
}

TEST_F(QemuBackend, removeAllSnapshotsFromTheImage)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    // The sole reason to register this callback is to make the extract_snapshot_tags function get a
    // non-empty snapshot list input, so we can cover the for loops
    mpt::MockProcessFactory::Callback snapshot_list_callback = [](mpt::MockProcess* process) {
        if (process->program().contains("qemu-img") && process->arguments().contains("snapshot") &&
            process->arguments().contains("-l"))
        {
            constexpr auto snapshot_list_output_stream =
                R"(Snapshot list:
            ID        TAG               VM SIZE                DATE     VM CLOCK     ICOUNT
            2         @s2               0 B     2024-06-11 23:22:59 00:00:00.000          0
            3         @s3               0 B     2024-06-12 12:30:37 00:00:00.000          0)";

            EXPECT_CALL(*process, read_all_standard_output())
                .WillOnce(Return(QByteArray{snapshot_list_output_stream}));
        }
    };

    process_factory->register_callback(snapshot_list_callback);
    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    const mp::QemuVirtualMachine machine{default_description,
                                         mock_qemu_platform.get(),
                                         stub_monitor,
                                         key_provider,
                                         instance_dir.path(),
                                         true};

    const std::vector<mpt::MockProcessFactory::ProcessInfo> processes =
        process_factory->process_list();

    EXPECT_GE(processes.size(), 2);
    const auto lastProcessInfo = processes.back();
    const auto last2ndProcessInfo = processes[processes.size() - 2];

    EXPECT_TRUE(lastProcessInfo.command == "qemu-img" && lastProcessInfo.arguments.contains("-d") &&
                lastProcessInfo.arguments.contains("@s3"));
    EXPECT_TRUE(last2ndProcessInfo.command == "qemu-img" &&
                last2ndProcessInfo.arguments.contains("-d") &&
                last2ndProcessInfo.arguments.contains("@s2"));
}

TEST_F(QemuBackend, cloneCopiesRelevantFiles)
{
    EXPECT_CALL(*mock_qemu_platform_factory, make_qemu_platform(_)).WillOnce([this](auto...) {
        return std::move(mock_qemu_platform);
    });

    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};
    const mpt::MockCloudInitFileOps::GuardedMock mock_cloud_init_file_ops_injection =
        mpt::MockCloudInitFileOps::inject<NiceMock>();
    EXPECT_CALL(*mock_cloud_init_file_ops_injection.first, update_identifiers(_, _, _, _)).Times(1);

    const QString instance_sub_dir = "vault/instances/";
    namespace fs = std::filesystem;
    const fs::path instances_dir{data_dir.filePath(instance_sub_dir).toStdString()};
    constexpr auto* src_vm_name = "dummy_src_name";
    constexpr auto* dest_vm_name = "dummy_dest_name";

    const fs::path src_vm_dir = instances_dir / src_vm_name;
    const fs::path dest_vm_dir = instances_dir / dest_vm_name;
    std::list<std::string> source_files{"dummy.img",
                                        "dummy.qcow2",
                                        "dummy.iso",
                                        "snapshot-count",
                                        "snapshot-head",
                                        "0001.snapshot.json"};

    std::unordered_set<std::string> expected_files{"dummy.img", "dummy.qcow2", "dummy.iso"};

    fs::create_directories(src_vm_dir);
    // create the files with the parent directory in place
    for (const auto& file : source_files)
    {
        std::ofstream(src_vm_dir / file);
    }

    EXPECT_TRUE(
        backend.clone_bare_vm({}, {}, src_vm_name, dest_vm_name, {}, key_provider, stub_monitor));

    std::unordered_set<std::string> actual_files;
    for (const auto& file : fs::directory_iterator(dest_vm_dir))
    {
        if (fs::is_regular_file(file.status()))
        {
            actual_files.insert(file.path().filename().string());
        }
    }

    EXPECT_EQ(actual_files, expected_files);
}

TEST(QemuPlatform, baseQemuPlatformReturnsExpectedValues)
{
    mpt::MockQemuPlatform qemu_platform;
    mp::VirtualMachineDescription vm_desc;

    EXPECT_CALL(qemu_platform, vmstate_platform_args()).WillOnce([&qemu_platform] {
        return qemu_platform.QemuPlatform::vmstate_platform_args();
    });
    EXPECT_CALL(qemu_platform, get_directory_name()).WillOnce([&qemu_platform] {
        return qemu_platform.QemuPlatform::get_directory_name();
    });

    EXPECT_TRUE(qemu_platform.vmstate_platform_args().isEmpty());
    EXPECT_TRUE(qemu_platform.get_directory_name().isEmpty());
}
