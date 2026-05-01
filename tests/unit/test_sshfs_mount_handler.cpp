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

#include "common.h"
#include "mock_environment_helpers.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_process_factory.h"
#include "mock_server_reader_writer.h"
#include "mock_ssh_session.h"
#include "mock_virtual_machine.h"
#include "stub_ssh_key_provider.h"

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/sshfs_mount/sshfs_mount_handler.h>
#include <multipass/vm_mount.h>

#include <QCoreApplication>
#include <QTimer>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
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
            .WillOnce(
                Return(mp::fs::file_status{mp::fs::file_type::directory, mp::fs::perms::all}));
        ON_CALL(mock_file_ops, weakly_canonical).WillByDefault([](const mp::fs::path& path) {
            // Not using weakly_canonical since we don't want symlink resolution on fake paths
            return mp::fs::absolute(path);
        });
    }

    void TearDown() override
    {
        // Deliberately spin the event loop to ensure all deleteLater()'ed QObjects are cleaned up,
        // so the mock tests are performed
        qApp->processEvents(QEventLoop::AllEvents);
    }

    void expect_and_fail_ssh_command(mpt::MockSSHSession& session, const std::string& cmd)
    {
        EXPECT_CALL(session, exec(HasSubstr(cmd), _)).WillOnce([] {
            auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
            EXPECT_CALL(*proc, exit_code).WillOnce(Return(1));
            return proc;
        });
    }

    mpt::StubSSHKeyProvider key_provider;
    std::string source_path{mp::fs::absolute("/my/source/path").string()},
        target_path{"/the/target/path"};
    mp::id_mappings gid_mappings{{1, 2}, {3, 4}}, uid_mappings{{5, -1}, {6, 10}};
    mp::VMMount mount{source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Classic};
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(default_log_level);
    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;
    NiceMock<mpt::MockVirtualMachine> vm;
    std::unique_ptr<mpt::MockProcessFactory::Scope> factory = mpt::MockProcessFactory::Inject();

    mpt::MockProcessFactory::Callback sshfs_prints_connected = [](mpt::MockProcess* process) {
        // Have "sshfs_server" print "Connected" to its stdout after short delay
        ON_CALL(*process, read_all_standard_output).WillByDefault(Return("Connected"));
        QTimer::singleShot(1, process, [process] { emit process->ready_read_standard_output(); });
        // Ensure process_state() does not have an exit code set (i.e. still running)
        ON_CALL(*process, process_state).WillByDefault(Return(mp::ProcessState{}));
    };
};

TEST_F(SSHFSMountHandlerTest, mountCreatesSshfsProcess)
{
    factory->register_callback(sshfs_server_callback(sshfs_prints_connected));

    mpt::MockVirtualMachine mock_vm{};
    EXPECT_CALL(mock_vm, ssh_port()).Times(1);
    EXPECT_CALL(mock_vm, ssh_hostname()).Times(1);
    EXPECT_CALL(mock_vm, ssh_username()).Times(1);
    EXPECT_CALL(mock_vm, new_ssh_session());

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
    EXPECT_TRUE(sshfs_command.arguments[5] == "6:10,5:-1," ||
                sshfs_command.arguments[5] == "5:-1,6:10,");
    EXPECT_TRUE(sshfs_command.arguments[6] == "3:4,1:2," ||
                sshfs_command.arguments[6] == "1:2,3:4,");

    const QString log_level_as_string{QString::number(static_cast<int>(default_log_level))};
    EXPECT_EQ(sshfs_command.arguments[7], log_level_as_string);
}

TEST_F(SSHFSMountHandlerTest, sshfsProcessFailingWithReturnCode9CausesException)
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

TEST_F(SSHFSMountHandlerTest, sshfsProcessFailingCausesRuntimeException)
{
    factory->register_callback(sshfs_server_callback([](mpt::MockProcess* process) {
        mp::ProcessState exit_state;
        exit_state.exit_code = 1;
        // Have "sshfs_server" die after short delay
        ON_CALL(*process, read_all_standard_error()).WillByDefault(Return("Whoopsie"));
        QTimer::singleShot(100, process, [process, exit_state]() {
            emit process->finished(exit_state);
        });
        ON_CALL(*process, process_state()).WillByDefault(Return(exit_state));
    }));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    MP_EXPECT_THROW_THAT(sshfs_mount_handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(StrEq("Process returned exit code: 1: Whoopsie")));
}

TEST_F(SSHFSMountHandlerTest, stopTerminatesSshfsProcess)
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

TEST_F(SSHFSMountHandlerTest, throwsInstallSshfsWhichSnapFails)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_and_fail_ssh_command(*session, "which snap");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), std::runtime_error);
}

TEST_F(SSHFSMountHandlerTest, throwsInstallSshfsNoSnapDirFails)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    EXPECT_CALL(*session, exec(HasSubstr("which snap"), _));
    expect_and_fail_ssh_command(*session, "snap list multipass-sshfs");
    expect_and_fail_ssh_command(*session, "[ -e /snap ]");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), std::runtime_error);
}

TEST_F(SSHFSMountHandlerTest, throwsInstallSshfsSnapInstallFails)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    EXPECT_CALL(*session, exec(HasSubstr("which snap"), _));
    expect_and_fail_ssh_command(*session, "snap list multipass-sshfs");
    EXPECT_CALL(*session, exec(HasSubstr("[ -e /snap ]"), _));
    expect_and_fail_ssh_command(*session, "snap install multipass-sshfs");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server), mp::SSHFSMissingError);
}

TEST_F(SSHFSMountHandlerTest, installSshfsTimeoutLogsInfo)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    EXPECT_CALL(*session, exec(HasSubstr("which snap"), _));
    expect_and_fail_ssh_command(*session, "snap list multipass-sshfs");
    EXPECT_CALL(*session, exec(HasSubstr("[ -e /snap ]"), _));
    EXPECT_CALL(*session, exec(HasSubstr("snap install multipass-sshfs"), _)).WillOnce([] {
        auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*proc, exit_code).WillOnce([](std::chrono::milliseconds timeout) -> int {
            throw mp::SSHProcessTimeoutException{"sudo snap install multipass-sshfs", timeout};
        });
        return proc;
    });
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(mpl::Level::error,
            StrEq("sshfs-mount-handler"),
            AllOf(HasSubstr("Could not install 'multipass-sshfs'"), HasSubstr("timed out"))));

    mp::SSHFSMountHandler sshfs_mount_handler{&vm, &key_provider, target_path, mount};
    EXPECT_THROW(sshfs_mount_handler.activate(&server, std::chrono::milliseconds(1)),
                 mp::SSHFSMissingError);
}
} // namespace
