/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/sshfs_mount/sshfs_mounts.h>

#include "mock_environment_helpers.h"
#include "mock_logger.h"
#include "mock_process_factory.h"
#include "mock_virtual_machine.h"
#include "stub_ssh_key_provider.h"

#include <QCoreApplication>
#include <QTimer>
#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

const multipass::logging::Level default_log_level = multipass::logging::Level::debug;

struct SSHFSMountsTest : public ::Test
{
    void TearDown() override
    {
        // Deliberately spin the event loop to ensure all deleteLater()'ed QObjects are cleaned up, so
        // the mock tests are performed
        qApp->processEvents(QEventLoop::AllEvents);
    }

    mpt::StubSSHKeyProvider key_provider;
    std::string source_path{"/my/source/path"}, target_path{"/the/target/path"};
    std::unordered_map<int, int> gid_map{{1, 2}, {3, 4}}, uid_map{{5, -1}, {6, 10}};
    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(default_log_level);

    mpt::MockProcessFactory::Callback sshfs_prints_connected = [](mpt::MockProcess* process) {
        if (process->program().contains("sshfs_server"))
        {
            // Have "sshfs_server" print "Connected" to its stdout after short delay
            ON_CALL(*process, read_all_standard_output()).WillByDefault(Return("Connected"));
            QTimer::singleShot(100, process, [process]() { emit process->ready_read_standard_output(); });

            // Ensure process_state() does not have an exit code set (i.e. still running)
            mp::ProcessState running_state;
            ON_CALL(*process, process_state()).WillByDefault(Return(running_state));
        }
    };
};

TEST_F(SSHFSMountsTest, mount_creates_sshfs_process)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(sshfs_prints_connected);

    mp::SSHFSMounts sshfs_mounts(key_provider);

    mpt::MockVirtualMachine vm{"my_instance"};
    EXPECT_CALL(vm, ssh_port());
    EXPECT_CALL(vm, ssh_hostname());
    EXPECT_CALL(vm, ssh_username());

    sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map);

    ASSERT_EQ(factory->process_list().size(), 1u);
    auto sshfs_command = factory->process_list()[0];
    EXPECT_TRUE(sshfs_command.command.endsWith("sshfs_server"));

    ASSERT_EQ(sshfs_command.arguments.size(), 8);
    EXPECT_EQ(sshfs_command.arguments[0], "localhost");
    EXPECT_EQ(sshfs_command.arguments[1], "42");
    EXPECT_EQ(sshfs_command.arguments[2], "ubuntu");
    EXPECT_EQ(sshfs_command.arguments[3], "/my/source/path");
    EXPECT_EQ(sshfs_command.arguments[4], "/the/target/path");
    // Ordering of the next 2 options not guaranteed, hence the or-s.
    EXPECT_TRUE(sshfs_command.arguments[5] == "6:10,5:-1," || sshfs_command.arguments[5] == "5:-1,6:10,");
    EXPECT_TRUE(sshfs_command.arguments[6] == "3:4,1:2," || sshfs_command.arguments[6] == "1:2,3:4,");

    const QString log_level_as_string{QString::number(static_cast<int>(default_log_level))};
    EXPECT_EQ(sshfs_command.arguments[7], log_level_as_string);
}

TEST_F(SSHFSMountsTest, sshfs_process_failing_with_return_code_9_causes_exception)
{
    auto factory = mpt::MockProcessFactory::Inject();

    mpt::MockProcessFactory::Callback sshfs_fails_with_exit_code_nine = [](mpt::MockProcess* process) {
        if (process->program().contains("sshfs_server"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 9;

            // Have "sshfs_server" die after short delay
            QTimer::singleShot(100, process, [process]() { emit process->finished({9, {}}); });

            ON_CALL(*process, process_state()).WillByDefault(Return(exit_state));
        }
    };
    factory->register_callback(sshfs_fails_with_exit_code_nine);

    mp::SSHFSMounts sshfs_mounts(key_provider);
    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    EXPECT_THROW(sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map), mp::SSHFSMissingError);

    ASSERT_EQ(factory->process_list().size(), 1u);
    auto sshfs_command = factory->process_list()[0];
    EXPECT_TRUE(sshfs_command.command.endsWith("sshfs_server"));
}

TEST_F(SSHFSMountsTest, sshfs_process_failing_causes_runtime_exception)
{
    auto factory = mpt::MockProcessFactory::Inject();

    mpt::MockProcessFactory::Callback sshfs_fails = [](mpt::MockProcess* process) {
        if (process->program().contains("sshfs_server"))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 1;

            // Have "sshfs_server" die after short delay
            ON_CALL(*process, read_all_standard_error()).WillByDefault(Return("Whoopsie"));
            QTimer::singleShot(100, process, [process, exit_state]() { emit process->finished(exit_state); });

            ON_CALL(*process, process_state()).WillByDefault(Return(exit_state));
        }
    };
    factory->register_callback(sshfs_fails);

    mp::SSHFSMounts sshfs_mounts(key_provider);
    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    EXPECT_THROW(try { sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map); } catch (
                     const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Process returned exit code: 1: Whoopsie");
        throw;
    },
                 std::runtime_error);
}

TEST_F(SSHFSMountsTest, stop_terminates_sshfs_process)
{
    auto factory = mpt::MockProcessFactory::Inject();
    mpt::MockProcessFactory::Callback sshfs_fails = [this](mpt::MockProcess* process) {
        sshfs_prints_connected(process);

        if (process->program().contains("sshfs_server"))
        {
            EXPECT_CALL(*process, terminate);
        }
    };
    factory->register_callback(sshfs_fails);

    mp::SSHFSMounts sshfs_mounts(key_provider);

    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map);
    int ret = sshfs_mounts.stop_mount(vm.vm_name, target_path);
    ASSERT_TRUE(ret);
}

TEST_F(SSHFSMountsTest, stop_all_mounts_terminates_all_sshfs_processes)
{
    auto factory = mpt::MockProcessFactory::Inject();
    mpt::MockProcessFactory::Callback sshfs_fails = [this](mpt::MockProcess* process) {
        sshfs_prints_connected(process);

        if (process->program().contains("sshfs_server"))
        {
            EXPECT_CALL(*process, terminate);
        }
    };
    factory->register_callback(sshfs_fails);

    mp::SSHFSMounts sshfs_mounts(key_provider);

    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    sshfs_mounts.start_mount(&vm, "/source/one", "/target/one", gid_map, uid_map);
    sshfs_mounts.start_mount(&vm, "/source/two", "/target/two", gid_map, uid_map);
    sshfs_mounts.start_mount(&vm, "/source/three", "/target/three", gid_map, uid_map);

    sshfs_mounts.stop_all_mounts_for_instance(vm.vm_name);
}

TEST_F(SSHFSMountsTest, has_instance_already_mounted_returns_true_when_found)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(sshfs_prints_connected);

    mp::SSHFSMounts sshfs_mounts(key_provider);

    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map);

    EXPECT_TRUE(sshfs_mounts.has_instance_already_mounted(vm.vm_name, target_path));
}

TEST_F(SSHFSMountsTest, has_instance_already_mounted_returns_false_when_no_such_mount)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(sshfs_prints_connected);

    mp::SSHFSMounts sshfs_mounts(key_provider);

    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map);

    EXPECT_FALSE(sshfs_mounts.has_instance_already_mounted(vm.vm_name, "/bad/path"));
}

TEST_F(SSHFSMountsTest, has_instance_already_mounted_returns_false_when_no_such_instance)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(sshfs_prints_connected);

    mp::SSHFSMounts sshfs_mounts(key_provider);

    NiceMock<mpt::MockVirtualMachine> vm{"my_instance"};

    sshfs_mounts.start_mount(&vm, source_path, target_path, gid_map, uid_map);

    EXPECT_FALSE(sshfs_mounts.has_instance_already_mounted("bad_vm_name", target_path));
}
