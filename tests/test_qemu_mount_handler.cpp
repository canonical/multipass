/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_server_reader_writer.h"
#include "mock_ssh_process_exit_status.h"
#include "mock_ssh_test_fixture.h"
#include "mock_virtual_machine.h"
#include "qemu_mount_handler.h"
#include "stub_ssh_key_provider.h"
#include "stub_virtual_machine.h"

#include <multipass/utils.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

typedef std::vector<std::pair<std::string, std::string>> CommandVector;

const multipass::logging::Level default_log_level = multipass::logging::Level::debug;

namespace
{
struct QemuMountHandlerTest : public ::Test
{
    void start_mount(std::optional<std::string> target = std::nullopt)
    {
        mp::QemuMountHandler qemu_mount_handler(key_provider);

        qemu_mount_handler.start_mount(&vm, &server, target.value_or(default_target));
    }

    auto make_exec_that_fails_for(const std::vector<std::string>& expected_cmds, bool& invoked)
    {
        auto request_exec = [this, expected_cmds, &invoked](ssh_channel, const char* raw_cmd) {
            std::string cmd{raw_cmd};

            for (const auto& expected_cmd : expected_cmds)
            {
                if (cmd.find(expected_cmd) != std::string::npos)
                {
                    invoked = true;
                    exit_status_mock.return_exit_code(SSH_ERROR);
                }
            }
            return SSH_OK;
        };
        return request_exec;
    }

    // The 'invoked' parameter binds the execution and read mocks. We need a better mechanism to make them
    // cooperate better, i.e., make the reader read only when a command was issued.
    auto make_exec_to_check_commands(const CommandVector& commands, std::string::size_type& remaining,
                                     CommandVector::const_iterator& next_expected_cmd, std::string& output,
                                     bool& invoked, std::optional<std::string>& fail_cmd,
                                     std::optional<bool>& fail_invoked)
    {
        *fail_invoked = false;

        auto request_exec = [this, &commands, &remaining, &next_expected_cmd, &output, &invoked, &fail_cmd,
                             &fail_invoked](ssh_channel, const char* raw_cmd) {
            invoked = false;

            std::string cmd{raw_cmd};

            if (fail_cmd && cmd.find(*fail_cmd) != std::string::npos)
            {
                if (fail_invoked)
                {
                    *fail_invoked = true;
                }
                exit_status_mock.return_exit_code(SSH_ERROR);
            }
            else if (next_expected_cmd != commands.end())
            {
                // Check if the first element of the given commands list is what we are expecting. In that case,
                // give the correct answer. If not, check the rest of the list to see if we broke the execution
                // order.
                auto pred = [&cmd](auto it) { return cmd == it.first; };
                CommandVector::const_iterator found_cmd = std::find_if(next_expected_cmd, commands.end(), pred);

                if (found_cmd == next_expected_cmd)
                {
                    invoked = true;
                    output = next_expected_cmd->second;
                    remaining = output.size();
                    ++next_expected_cmd;

                    return SSH_OK;
                }
                else if (found_cmd != commands.end())
                {
                    output = found_cmd->second;
                    remaining = output.size();
                    ADD_FAILURE() << "\"" << (found_cmd->first) << "\" executed out of order; expected \""
                                  << next_expected_cmd->first << "\"";

                    return SSH_OK;
                }
            }

            // If the command list was entirely checked or if the executed command is not on the list, check the
            // default commands to see if there is an answer to the current command.
            auto it = default_cmds.find(cmd);
            if (it != default_cmds.end())
            {
                output = it->second;
                remaining = output.size();
                invoked = true;
            }

            return SSH_OK;
        };

        return request_exec;
    }

    auto make_channel_read_return(const std::string& output, std::string::size_type& remaining, bool& prereq_invoked)
    {
        auto channel_read = [&output, &remaining, &prereq_invoked](ssh_channel, void* dest, uint32_t count,
                                                                   int is_stderr, int) {
            if (!prereq_invoked)
                return 0u;
            const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
            const auto begin = output.begin() + output.size() - remaining;
            std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
            remaining -= num_to_copy;
            return num_to_copy;
        };
        return channel_read;
    }

    void test_command_execution(const CommandVector& commands, std::optional<std::string> target = std::nullopt,
                                std::optional<std::string> fail_cmd = std::nullopt,
                                std::optional<bool> fail_invoked = std::nullopt)
    {
        bool invoked{false};
        std::string output;
        auto remaining = output.size();
        CommandVector::const_iterator next_expected_cmd = commands.begin();

        auto channel_read = make_channel_read_return(output, remaining, invoked);
        REPLACE(ssh_channel_read_timeout, channel_read);

        auto request_exec = make_exec_to_check_commands(commands, remaining, next_expected_cmd, output, invoked,
                                                        fail_cmd, fail_invoked);
        REPLACE(ssh_channel_request_exec, request_exec);

        start_mount(target.value_or(default_target));

        EXPECT_TRUE(next_expected_cmd == commands.end()) << "\"" << next_expected_cmd->first << "\" not executed";
    }

    mpt::StubSSHKeyProvider key_provider;
    std::string default_source{"source"}, default_target{"target"};
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
    mp::id_mappings gid_mappings{{1, 2}}, uid_mappings{{5, -1}};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(default_log_level);
    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mpt::ExitStatusMock exit_status_mock;
    mpt::MockVirtualMachine vm{"my_instance"};

    const std::unordered_map<std::string, std::string> default_cmds{
        {"echo $PWD/target", "/home/ubuntu/target\n"},
        {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'\n",
         "/home/ubuntu/target\n"},
        {"id -u", "1000\n"},
        {"id -g", "1000\n"},
        {"sudo mount -t 9p /my/source/path -o trans=virtio,version=9p2000.L,msize=536870912\n", "don't care\n"},
        {"sudo umount /my/source/path\n", "don't care\n"}};
};

// Mocks an incorrect return from a given command.
struct QemuMountFail : public QemuMountHandlerTest, public testing::WithParamInterface<std::string>
{
};

struct QemuMountExecute : public QemuMountHandlerTest,
                          public testing::WithParamInterface<std::pair<std::string, CommandVector>>
{
};

struct QemuMountExecuteAndNoFail
    : public QemuMountHandlerTest,
      public testing::WithParamInterface<std::tuple<std::string, CommandVector, std::string>>
{
};

} // namespace

//
// Define some parameterized test fixtures.
//

TEST_P(QemuMountFail, testFailedInvocation)
{
    bool invoked_cmd{false};
    std::string output;
    std::string::size_type remaining;

    auto channel_read = make_channel_read_return(output, remaining, invoked_cmd);
    REPLACE(ssh_channel_read_timeout, channel_read);

    CommandVector empty;
    CommandVector::const_iterator it = empty.end();
    std::optional<std::string> fail_cmd = std::make_optional(GetParam());
    std::optional<bool> invoked_fail = std::make_optional(false);
    auto request_exec = make_exec_to_check_commands(empty, remaining, it, output, invoked_cmd, fail_cmd, invoked_fail);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(start_mount(), std::runtime_error);
    EXPECT_TRUE(*invoked_fail);
}

TEST_P(QemuMountExecute, testSuccesfulInvocation)
{
    std::string target = GetParam().first;
    CommandVector commands = GetParam().second;

    test_command_execution(commands, target);
}

TEST_P(QemuMountExecuteAndNoFail, testSuccesfulInvocationAndFail)
{
    std::string target = std::get<0>(GetParam());
    CommandVector commands = std::get<1>(GetParam());
    std::string fail_command = std::get<2>(GetParam());

    ASSERT_NO_THROW(test_command_execution(commands, target, fail_command));
}

//
// Instantiate the parameterized tests suites from above.
//

INSTANTIATE_TEST_SUITE_P(QemuMountThrowWhenError, QemuMountFail,
                         testing::Values("mkdir", "chown", "id -u", "id -g", "cd", "echo $PWD", "mount"));

INSTANTIATE_TEST_SUITE_P(
    QemuMountSuccess, QemuMountExecute,
    testing::Values(
        // Commands to check that it works with a path containing a space.
        std::make_pair("unsc infinity", CommandVector{{"echo $PWD/unsc\\ infinity", "/home/ubuntu/unsc infinity\n"},
                                                      {"sudo /bin/bash -c 'P=\"/home/ubuntu/unsc infinity\"; while [ ! "
                                                       "-d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
                                                       "/home/ubuntu/\n"}}),
        // Commands to check that the ~ expansion works.
        std::make_pair("~/target", CommandVector{{"echo ~/target", "/home/ubuntu/target\n"},
                                                 {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" "
                                                  "]; do P=\"${P%/*}\"; done; echo $P/'",
                                                  "/home/ubuntu/\n"}}),
        // Commands to check that the ~user expansion works (assuming that user exists).
        std::make_pair("~ubuntu/target", CommandVector{{"echo ~ubuntu/target", "/home/ubuntu/target\n"},
                                                       {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d "
                                                        "\"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
                                                        "/home/ubuntu/\n"}}),
        // Commands to check that the handler works if an absolute path is given.
        std::make_pair("/home/ubuntu/target", CommandVector{{"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! "
                                                             "-d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
                                                             "/home/ubuntu/\n"}}),
        // Commands to check that it works for a nonexisting path.
        std::make_pair("/nonexisting/path", CommandVector{{"sudo /bin/bash -c 'P=\"/nonexisting/path\"; while [ ! -d "
                                                           "\"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
                                                           "/\n"}})));

// Commands to test that when a mount path already exists, no mkdir nor chown is ran.
INSTANTIATE_TEST_SUITE_P(
    QemuMountSuccessAndAvoidCommands, QemuMountExecuteAndNoFail,
    testing::Values(std::make_tuple("target",
                                    CommandVector{{"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" "
                                                   "]; do P=\"${P%/*}\"; done; echo $P/'",
                                                   "/home/ubuntu/target/\n"}},
                                    "mkdir"),
                    std::make_tuple("target",
                                    CommandVector{{"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" "
                                                   "]; do P=\"${P%/*}\"; done; echo $P/'",
                                                   "/home/ubuntu/target/\n"}},
                                    "chown")));

TEST_F(QemuMountHandlerTest, mountFailsOnNotStoppedState)
{
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::running));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};

    EXPECT_THROW(
        try { qemu_mount_handler.init_mount(&vm, default_target, mount); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Please shutdown virtual machine before defining native mount.");
            throw;
        },
        std::runtime_error);
}

TEST_F(QemuMountHandlerTest, mountFailsOnNonExistentPath)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(false));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};

    EXPECT_THROW(
        try { qemu_mount_handler.init_mount(&vm, default_target, mount); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Mount path \"source\" does not exist.");
            throw;
        },
        std::runtime_error);
}

TEST_F(QemuMountHandlerTest, mountFailsOnMultipleUIDs)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{default_source, {{1, 2}, {3, 4}}, {{5, -1}, {6, 10}}, mp::VMMount::MountType::Performance};

    EXPECT_THROW(
        try { qemu_mount_handler.init_mount(&vm, default_target, mount); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Only one mapping per native mount allowed.");
            throw;
        },
        std::runtime_error);
}

TEST_F(QemuMountHandlerTest, hasInstanceAlreadyMountedReturnsTrueWhenFound)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};
    EXPECT_CALL(vm, add_vm_mount(default_target, mount)).WillOnce(Return());

    qemu_mount_handler.init_mount(&vm, default_target, mount);

    EXPECT_TRUE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, default_target));
}

TEST_F(QemuMountHandlerTest, hasInstanceAlreadyMountedReturnsFalseWhenFound)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};
    EXPECT_CALL(vm, add_vm_mount(default_target, mount)).WillOnce(Return());

    qemu_mount_handler.init_mount(&vm, default_target, mount);

    EXPECT_FALSE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/bad/path"));
}

TEST_F(QemuMountHandlerTest, stopNonExistentMountLogsMessageAndReturns)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("qemu-mount-handler")),
                    mpt::MockLogger::make_cstring_matcher(StrEq(
                        fmt::format("No native mount defined for \"{}\" serving '{}'", vm.vm_name, default_target)))));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    qemu_mount_handler.stop_mount(vm.vm_name, default_target);
}

TEST_F(QemuMountHandlerTest, stopAllMountsForInstanceWithNoMountsLogsMessageAndReturns)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("qemu-mount-handler")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq(fmt::format("No native mounts to stop for instance \"{}\"", vm.vm_name)))));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    qemu_mount_handler.stop_all_mounts_for_instance(vm.vm_name);
}

TEST_F(QemuMountHandlerTest, stopAllMountsDeletesAllMounts)
{
    bool invoked_cmd{false};
    std::string output;
    auto remaining = output.size();

    auto channel_read = make_channel_read_return(output, remaining, invoked_cmd);
    REPLACE(ssh_channel_read_timeout, channel_read);

    CommandVector empty;
    CommandVector::const_iterator it = empty.end();
    std::optional<std::string> fail_cmd = std::nullopt;
    std::optional<bool> invoked_fail = std::nullopt;
    auto request_exec = make_exec_to_check_commands(empty, remaining, it, output, invoked_cmd, fail_cmd, invoked_fail);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, add_vm_mount(_, _)).WillOnce(Return());
    EXPECT_CALL(vm, delete_vm_mount(_)).WillOnce(Return());

    const mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};
    mp::QemuMountHandler qemu_mount_handler(key_provider);

    qemu_mount_handler.init_mount(&vm, default_target, mount);
    qemu_mount_handler.start_mount(&vm, &server, default_target);

    EXPECT_TRUE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, default_target));
    qemu_mount_handler.stop_mount(vm.vm_name, default_target);
    EXPECT_FALSE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, default_target));
}
