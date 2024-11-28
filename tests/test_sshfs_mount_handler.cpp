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

#include "mock_ssh_test_fixture.h"

#include "common.h"
#include "mock_environment_helpers.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_process_factory.h"
#include "mock_server_reader_writer.h"
#include "mock_ssh_process_exit_status.h"
#include "mock_virtual_machine.h"
#include "stub_ssh_key_provider.h"
#include "stub_virtual_machine.h"

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/sshfs_mount/sshfs_mount_handler.h>
#include <multipass/vm_mount.h>

#include <thread>

#include <QCoreApplication>
#include <QTimer>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

const multipass::logging::Level default_log_level = multipass::logging::Level::debug;

auto sshfs_server_callback(mpt::MockProcessFactory::Callback callback)
{
    return [callback](mpt::MockProcess* process) {
        if (process->program().contains("sshfs_server"))
            callback(process);
    };
}

struct SSHFSMountHandlerTest : public ::Test
{
    SSHFSMountHandlerTest()
    {
        EXPECT_CALL(server, Write(_, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_file_ops, status)
            .WillOnce(Return(mp::fs::file_status{mp::fs::file_type::directory, mp::fs::perms::all}));
        ON_CALL(mock_file_ops, weakly_canonical).WillByDefault([](const mp::fs::path& path) {
            // Not using weakly_canonical since we don't want symlink resolution on fake paths
            return mp::fs::absolute(path);
        });
    }

    void TearDown() override
    {
        // Deliberately spin the event loop to ensure all deleteLater()'ed QObjects are cleaned up, so
        // the mock tests are performed
        qApp->processEvents(QEventLoop::AllEvents);
    }

    auto make_exec_that_fails_for(const std::vector<std::string>& expected_cmds, bool& invoked)
    {
        return [this, expected_cmds, &invoked](ssh_channel, const char* raw_cmd) {
            std::string cmd{raw_cmd};
            exit_status_mock.set_exit_status(exit_status_mock.success_status);

            for (const auto& expected_cmd : expected_cmds)
            {
                if (cmd.find(expected_cmd) != std::string::npos)
                {
                    invoked = true;
                    exit_status_mock.set_exit_status(exit_status_mock.failure_status);
                    break;
                }
            }
            return SSH_OK;
        };
    }

    mpt::StubSSHKeyProvider key_provider;
    std::string source_path{mp::fs::absolute("/my/source/path").string()}, target_path{"/the/target/path"};
    mp::id_mappings gid_mappings{{1, 2}, {3, 4}}, uid_mappings{{5, -1}, {6, 10}};
    mp::VMMount mount{source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Classic};
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(default_log_level);
    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mpt::ExitStatusMock exit_status_mock;
    mpt::StubVirtualMachine vm;
    std::unique_ptr<mpt::MockProcessFactory::Scope> factory = mpt::MockProcessFactory::Inject();

    mpt::MockProcessFactory::Callback sshfs_prints_connected = [](mpt::MockProcess* process) {
        // Have "sshfs_server" print "Connected" to its stdout after short delay
        ON_CALL(*process, read_all_standard_output).WillByDefault(Return("Connected"));
        QTimer::singleShot(1, process, [process] { emit process->ready_read_standard_output(); });
        // Ensure process_state() does not have an exit code set (i.e. still running)
        ON_CALL(*process, process_state).WillByDefault(Return(mp::ProcessState{}));
    };
};

TEST_F(SSHFSMountHandlerTest, mount_creates_sshfs_process)
{
    factory->register_callback(sshfs_server_callback(sshfs_prints_connected));

    mpt::MockVirtualMachine mock_vm{"my_instance"};
    EXPECT_CALL(mock_vm, ssh_port()).Times(3);
    EXPECT_CALL(mock_vm, ssh_hostname()).Times(3);
    EXPECT_CALL(mock_vm, ssh_username()).Times(3);

    mp::SSHFSMountHandler sshfs_mount_handler{&mock_vm, &key_provider, target_path, mount};
    sshfs_mount_handler.activate(&server);

    ASSERT_EQ(factory->process_list().size(), 1u);
    auto sshfs_command = factory->process_list()[0];
    EXPECT_TRUE(sshfs_command.command.endsWith("sshfs_server"));

    ASSERT_EQ(sshfs_command.arguments.size(), 8);
    EXPECT_EQ(sshfs_command.arguments[0], "localhost");
    EXPECT_EQ(sshfs_command.arguments[1], "42");
    EXPECT_EQ(sshfs_command.arguments[2], "ubuntu");
    EXPECT_EQ(sshfs_command.arguments[3].toStdString(), source_path);
    EXPECT_EQ(sshfs_command.arguments[4], "/the/target/path");
    // Ordering of the next 2 options not guaranteed, hence the or-s.
    EXPECT_TRUE(sshfs_command.arguments[5] == "6:10,5:-1," || sshfs_command.arguments[5] == "5:-1,6:10,");
    EXPECT_TRUE(sshfs_command.arguments[6] == "3:4,1:2," || sshfs_command.arguments[6] == "1:2,3:4,");

    const QString log_level_as_string{QString::number(static_cast<int>(default_log_level))};
    EXPECT_EQ(sshfs_command.arguments[7], log_level_as_string);
}

TEST_F(SSHFSMountHandlerTest, sshfs_process_failing_with_return_code_9_causes_exception)
{
    factory->register_callback(sshfs_server_callback([](mpt::MockProcess* process) {
        mp::ProcessState exit_state;
        exit_state.exit_code = 9;
        // Have "sshfs_server" die after short delay
        QTimer::singleShot(100, process, [process]() { emit process->finished({9, {}}); });
        ON_CALL(*process, process_state()).WillByDefault(Return(exit_state));
    }));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), mp::SSHFSMissingError);

    ASSERT_EQ(factory->process_list().size(), 1u);
    auto sshfs_command = factory->process_list()[0];
    EXPECT_TRUE(sshfs_command.command.endsWith("sshfs_server"));
}

TEST_F(SSHFSMountHandlerTest, sshfs_process_failing_causes_runtime_exception)
{
    factory->register_callback(sshfs_server_callback([](mpt::MockProcess* process) {
        mp::ProcessState exit_state;
        exit_state.exit_code = 1;
        // Have "sshfs_server" die after short delay
        ON_CALL(*process, read_all_standard_error()).WillByDefault(Return("Whoopsie"));
        QTimer::singleShot(100, process, [process, exit_state]() { emit process->finished(exit_state); });
        ON_CALL(*process, process_state()).WillByDefault(Return(exit_state));
    }));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    MP_EXPECT_THROW_THAT(sshfs_mount_handler.activate(&server), std::runtime_error,
                         mpt::match_what(StrEq("Process returned exit code: 1: Whoopsie")));
}

TEST_F(SSHFSMountHandlerTest, stop_terminates_sshfs_process)
{
    factory->register_callback(sshfs_server_callback([this](mpt::MockProcess* process) {
        sshfs_prints_connected(process);
        EXPECT_CALL(*process, terminate);
        EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
    }));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    sshfs_mount_handler.activate(&server);
    sshfs_mount_handler.deactivate();
}

TEST_F(SSHFSMountHandlerTest, throws_install_sshfs_which_snap_fails)
{
    auto invoked = false;
    REPLACE(ssh_channel_request_exec, make_exec_that_fails_for({"which snap"}, invoked));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SSHFSMountHandlerTest, throws_install_sshfs_no_snap_dir_fails)
{
    auto invoked = false;
    REPLACE(ssh_channel_request_exec,
            make_exec_that_fails_for({"[ -e /snap ]", "sudo snap list multipass-sshfs"}, invoked));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SSHFSMountHandlerTest, throws_install_sshfs_snap_install_fails)
{
    auto invoked = false;
    REPLACE(ssh_channel_request_exec,
            make_exec_that_fails_for({"sudo snap list multipass-sshfs", "sudo snap install multipass-sshfs"}, invoked));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), mp::SSHFSMissingError);
    EXPECT_TRUE(invoked);
}

TEST_F(SSHFSMountHandlerTest, install_sshfs_timeout_logs_info)
{
    ssh_channel_callbacks callbacks{nullptr};
    auto sleep = false;
    int exit_code;

    auto mocked_ssh_channel_request_exec = [&sleep, &exit_code](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        exit_code = SSH_OK;
        if (cmd == "sudo snap install multipass-sshfs")
            sleep = true;
        else if (cmd == "sudo snap list multipass-sshfs")
            exit_code = SSH_ERROR;

        return SSH_OK;
    };
    REPLACE(ssh_channel_request_exec, mocked_ssh_channel_request_exec);
    REPLACE(ssh_add_channel_callbacks, [&callbacks](ssh_channel, ssh_channel_callbacks cb) mutable {
        callbacks = cb;
        return SSH_OK;
    });

    auto mocked_ssh_event_dopoll = [&callbacks, &sleep, &exit_code](ssh_event, int timeout) {
        if (!callbacks)
            return SSH_ERROR;

        if (sleep)
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout + 1));
        else
            callbacks->channel_exit_status_function(nullptr, nullptr, exit_code, callbacks->userdata);

        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, mocked_ssh_event_dopoll);

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::error,
                    mpt::MockLogger::make_cstring_matcher(StrEq("sshfs-mount-handler")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("Could not install 'multipass-sshfs' in 'stub'"), HasSubstr("timed out")))));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server, std::chrono::milliseconds(1)), mp::SSHFSMissingError);
}
