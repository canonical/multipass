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
#include "mock_logger.h"
#include "mock_ssh_process_exit_status.h"
#include "sftp_server_test_fixture.h"
#include "signal.h"
#include "stub_ssh_key_provider.h"

#include <src/sshfs_mount/sshfs_mount.h>

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>

#include <algorithm>
#include <iterator>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

typedef std::vector<std::pair<std::string, std::string>> CommandVector;

namespace
{
struct SshfsMount : public mp::test::SftpServerTest
{
    mp::SshfsMount make_sshfsmount(std::optional<std::string> target = std::nullopt)
    {
        mp::SSHSession session{"a", 42, "ubuntu", key_provider};
        return {std::move(session), default_source, target.value_or(default_target), default_mappings,
                default_mappings};
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
                    exit_status_mock.set_exit_status(exit_status_mock.failure_status);
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
                exit_status_mock.set_exit_status(exit_status_mock.failure_status);
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

        make_sshfsmount(target.value_or(default_target));

        EXPECT_TRUE(next_expected_cmd == commands.end()) << "\"" << next_expected_cmd->first << "\" not executed";
    }

    mpt::ExitStatusMock exit_status_mock;

    std::string default_source{"source"};
    std::string default_target{"target"};
    mp::id_mappings default_mappings;
    int default_id{1000};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    const mpt::StubSSHKeyProvider key_provider;

    const std::unordered_map<std::string, std::string> default_cmds{
        {"snap run multipass-sshfs.env", "LD_LIBRARY_PATH=/foo/bar\nSNAP=/baz\n"},
        {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -V", "FUSE library version: 3.0.0\n"},
        {"echo $PWD/target", "/home/ubuntu/target\n"},
        {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
         "/home/ubuntu/\n"},
        {"id -u", "1000\n"},
        {"id -g", "1000\n"},
        {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -o slave -o transform_symlinks -o allow_other -o "
         "Compression=no -o dcache_timeout=3 :\"source\" "
         "\"target\"",
         "don't care\n"}};
};

// Mocks an incorrect return from a given command.
struct SshfsMountFail : public SshfsMount, public testing::WithParamInterface<std::string>
{
};

// Mocks correct execution of a given vector of commands/answers. The first string specifies the parameter with
// which make_sshfsmount mock is called.
struct SshfsMountExecute : public SshfsMount, public testing::WithParamInterface<std::pair<std::string, CommandVector>>
{
};

struct SshfsMountExecuteAndNoFail
    : public SshfsMount,
      public testing::WithParamInterface<std::tuple<std::string, CommandVector, std::string>>
{
};

// Mocks the server raising a std::invalid_argument.
struct SshfsMountExecuteThrowInvArg : public SshfsMount, public testing::WithParamInterface<CommandVector>
{
};

// Mocks the server raising a std::runtime_error.
struct SshfsMountExecuteThrowRuntErr : public SshfsMount, public testing::WithParamInterface<CommandVector>
{
};

} // namespace

//
// Define some parameterized test fixtures.
//

TEST_P(SshfsMountFail, test_failed_invocation)
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

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(*invoked_fail);
}

TEST_P(SshfsMountExecute, test_successful_invocation)
{
    std::string target = GetParam().first;
    CommandVector commands = GetParam().second;

    test_command_execution(commands, target);
}

TEST_P(SshfsMountExecuteAndNoFail, test_successful_invocation_and_fail)
{
    std::string target = std::get<0>(GetParam());
    CommandVector commands = std::get<1>(GetParam());
    std::string fail_command = std::get<2>(GetParam());

    ASSERT_NO_THROW(test_command_execution(commands, target, fail_command));
}

TEST_P(SshfsMountExecuteThrowInvArg, test_invalid_arg_when_executing)
{
    EXPECT_THROW(test_command_execution(GetParam()), std::invalid_argument);
}

TEST_P(SshfsMountExecuteThrowRuntErr, test_runtime_error_when_executing)
{
    EXPECT_THROW(test_command_execution(GetParam()), std::runtime_error);
}

//
// Instantiate the parameterized tests suites from above.
//

INSTANTIATE_TEST_SUITE_P(SshfsMountThrowWhenError, SshfsMountFail,
                         testing::Values("mkdir", "chown", "id -u", "id -g", "cd", "echo $PWD"));

// Commands to check that a version of FUSE smaller that 3 gives a correct answer.
CommandVector old_fuse_cmds = {
    {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -V", "FUSE library version: 2.9.0\n"},
    {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -o slave -o transform_symlinks -o "
     "allow_other -o Compression=no -o nonempty -o cache=no :\"source\" \"/home/ubuntu/target\"",
     "don't care\n"}};

// Commands to check that a version of FUSE at least 3.0.0 gives a correct answer.
CommandVector new_fuse_cmds = {{"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -V", "FUSE library version: 3.0.0\n"},
                               {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -o slave -o transform_symlinks -o "
                                "allow_other -o Compression=no -o dir_cache=no :\"source\" \"/home/ubuntu/target\"",
                                "don't care\n"}};

// Commands to check that an unknown version of FUSE gives a correct answer.
CommandVector unk_fuse_cmds = {{"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -V", "weird fuse version\n"},
                               {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -o slave -o transform_symlinks -o "
                                "allow_other -o Compression=no :\"source\" \"/home/ubuntu/target\"",
                                "don't care\n"}};

// Commands to check that the server correctly creates the mount target.
CommandVector exec_cmds = {
    {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
     "/home/ubuntu/\n"},
    {"sudo /bin/bash -c 'cd \"/home/ubuntu/\" && mkdir -p \"target\"'", "\n"},
    {"sudo /bin/bash -c 'cd \"/home/ubuntu/\" && chown -R 1000:1000 \"target\"'", "\n"}};

// Commands to check that it works with a path containing a space.
CommandVector space_cmds = {
    {"echo $PWD/space\\ odyssey", "/home/ubuntu/space odyssey\n"},
    {"sudo /bin/bash -c 'P=\"/home/ubuntu/space odyssey\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
     "/home/ubuntu/\n"}};

// Commands to check that the ~ expansion works.
CommandVector tilde1_cmds = {
    {"echo ~/target", "/home/ubuntu/target\n"},
    {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
     "/home/ubuntu/\n"}};

// Commands to check that the ~user expansion works (assuming that user exists).
CommandVector tilde2_cmds = {
    {"echo ~ubuntu/target", "/home/ubuntu/target\n"},
    {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
     "/home/ubuntu/\n"}};

// Commands to check that the server works if an absolute path is given.
CommandVector absolute_cmds = {
    {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
     "/home/ubuntu/\n"}};

// Commands to check that it works for a nonexisting path.
CommandVector nonexisting_path_cmds = {
    {"sudo /bin/bash -c 'P=\"/nonexisting/path\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'", "/\n"}};

// Check the execution of the CommandVector's above.
INSTANTIATE_TEST_SUITE_P(
    SshfsMountSuccess, SshfsMountExecute,
    testing::Values(std::make_pair("target", old_fuse_cmds), std::make_pair("target", new_fuse_cmds),
                    std::make_pair("target", unk_fuse_cmds), std::make_pair("target", exec_cmds),
                    std::make_pair("space odyssey", space_cmds), std::make_pair("~/target", tilde1_cmds),
                    std::make_pair("~ubuntu/target", tilde2_cmds), std::make_pair("/home/ubuntu/target", absolute_cmds),
                    std::make_pair("/nonexisting/path", nonexisting_path_cmds)));

// Commands to test that when a mount path already exists, no mkdir nor chown is ran.
CommandVector execute_no_mkdir_cmds = {
    {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=\"${P%/*}\"; done; echo $P/'",
     "/home/ubuntu/target/\n"}};

INSTANTIATE_TEST_SUITE_P(SshfsMountSuccessAndAvoidCommands, SshfsMountExecuteAndNoFail,
                         testing::Values(std::make_tuple("target", execute_no_mkdir_cmds, "mkdir"),
                                         std::make_tuple("target", execute_no_mkdir_cmds, "chown")));

// Check that some commands throw some exceptions.
CommandVector non_int_uid_cmds = {{"id -u", "ubuntu\n"}};
CommandVector non_int_gid_cmds = {{"id -g", "ubuntu\n"}};
CommandVector invalid_fuse_ver_cmds = {
    {"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -V", "FUSE library version: fu.man.chu\n"}};

INSTANTIATE_TEST_SUITE_P(SshfsMountThrowInvArg, SshfsMountExecuteThrowInvArg,
                         testing::Values(non_int_uid_cmds, non_int_gid_cmds));

INSTANTIATE_TEST_SUITE_P(SshfsMountThrowRuntErr, SshfsMountExecuteThrowRuntErr, testing::Values(invalid_fuse_ver_cmds));

//
// Finally, individual test fixtures.
//

TEST_F(SshfsMount, throws_when_sshfs_does_not_exist)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for({"sudo multipass-sshfs.env", "which sshfs"}, invoked);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), mp::SSHFSMissingError);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, unblocks_when_sftpserver_exits)
{
    mpt::Signal client_message;
    auto get_client_msg = [&client_message](sftp_session) {
        client_message.wait();
        return nullptr;
    };
    REPLACE(sftp_get_client_message, get_client_msg);

    bool stopped_ok = false;
    std::thread mount([&] {
        test_command_execution(CommandVector());
        stopped_ok = true;
    });

    client_message.signal();

    mount.join();
    EXPECT_TRUE(stopped_ok);
}

TEST_F(SshfsMount, blank_fuse_version_logs_error)
{
    CommandVector commands = {{"sudo env LD_LIBRARY_PATH=/foo/bar /baz/bin/sshfs -V", "FUSE library version:\n"}};

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::warning), mpt::MockLogger::make_cstring_matcher(StrEq("sshfs mount")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Unable to parse the FUSE library version"))));
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::debug), mpt::MockLogger::make_cstring_matcher(StrEq("sshfs mount")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq("Unable to parse the FUSE library version: FUSE library version:"))));

    test_command_execution(commands);
}
