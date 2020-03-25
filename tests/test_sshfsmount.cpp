/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include "sftp_server_test_fixture.h"
#include "signal.h"

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/optional.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sshfs_mount.h>
#include <multipass/utils.h>

#include <algorithm>
#include <gmock/gmock.h>
#include <iterator>
#include <unordered_set>
#include <vector>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

typedef std::vector<std::pair<std::string, std::string>> CommandVector;

namespace
{
struct SshfsMount : public mp::test::SftpServerTest
{
    SshfsMount()
    {
        channel_read.returnValue(0);
        channel_is_closed.returnValue(0);
    }

    mp::SshfsMount make_sshfsmount(mp::optional<std::string> target = mp::nullopt)
    {
        mp::SSHSession session{"a", 42};
        return {std::move(session), default_source, target.value_or(default_target), default_map, default_map};
    }

    auto make_exec_that_fails_for(const std::string& expected_cmd, bool& invoked)
    {
        auto request_exec = [this, expected_cmd, &invoked](ssh_channel, const char* raw_cmd) {
            std::string cmd{raw_cmd};
            if (cmd.find(expected_cmd) != std::string::npos)
            {
                invoked = true;
                exit_status_mock.return_exit_code(SSH_ERROR);
            }
            return SSH_OK;
        };
        return request_exec;
    }

    // The 'invoked' parameter binds the execution and read mocks. We need a better mechanism to make them
    // cooperate better, i.e., make the reader read only when a command was issued.
    auto make_exec_to_check_commands(const CommandVector& commands, std::string::size_type& remaining,
                                     CommandVector::const_iterator& next_expected_cmd, std::string& output,
                                     bool& invoked)
    {
        auto request_exec = [&commands, &remaining, &next_expected_cmd, &output, &invoked](ssh_channel,
                                                                                           const char* raw_cmd) {
            std::string cmd{raw_cmd};

            if (next_expected_cmd != commands.end())
            {
                // Check if the first of the remaining commands is being executed.
                if (cmd == next_expected_cmd->first)
                {
                    output = next_expected_cmd->second;
                    remaining = output.size();
                    ++next_expected_cmd;
                    invoked = true;
                }
                else
                {
                    // If not, then we have to check the rest of the remaining
                    // commands. If the executed command is there, it means that the
                    // executed commands do not follow the order of our expected
                    // command vector.
                    for (auto it = std::next(next_expected_cmd); it != commands.end(); ++it)
                    {
                        if (cmd == it->first)
                        {
                            output = it->second;
                            remaining = output.size();
                            ADD_FAILURE() << "\"" << (it->first) << "\" executed out of order; expected \""
                                          << next_expected_cmd->first << "\"";
                            break;
                        }
                    }
                }
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

    void test_command_execution(const CommandVector& commands, mp::optional<std::string> target = mp::nullopt)
    {
        bool invoked{false};
        std::string output;
        auto remaining = output.size();
        CommandVector::const_iterator next_expected_cmd = commands.begin();

        auto channel_read = make_channel_read_return(output, remaining, invoked);
        REPLACE(ssh_channel_read_timeout, channel_read);

        auto request_exec = make_exec_to_check_commands(commands, remaining, next_expected_cmd, output, invoked);
        REPLACE(ssh_channel_request_exec, request_exec);

        make_sshfsmount(target.value_or(default_target));

        EXPECT_TRUE(next_expected_cmd == commands.end()) << "\"" << next_expected_cmd->first << "\" not executed";
    }

    mpt::ExitStatusMock exit_status_mock;
    decltype(MOCK(ssh_channel_read_timeout)) channel_read{MOCK(ssh_channel_read_timeout)};
    decltype(MOCK(ssh_channel_is_closed)) channel_is_closed{MOCK(ssh_channel_is_closed)};

    std::string default_source{"source"};
    std::string default_target{"target"};
    std::unordered_map<int, int> default_map;
    int default_id{1000};
};
} // namespace

TEST_F(SshfsMount, throws_when_sshfs_does_not_exist)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for("which sshfs", invoked);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), mp::SSHFSMissingError);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, throws_when_unable_to_make_target_dir)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for("mkdir", invoked);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, throws_when_unable_to_chown)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for("chown", invoked);

    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, throws_when_unable_to_obtain_uid)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for("id -u", invoked);

    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, throws_when_uid_is_not_an_integer)
{
    bool invoked{false};
    auto request_exec = [&invoked](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("id -u") != std::string::npos)
        {
            invoked = true;
        }
        return SSH_OK;
    };
    REPLACE(ssh_channel_request_exec, request_exec);

    std::string output{"ubuntu"};
    auto remaining = output.size();
    auto channel_read = make_channel_read_return(output, remaining, invoked);
    REPLACE(ssh_channel_read_timeout, channel_read);

    EXPECT_THROW(make_sshfsmount(), std::invalid_argument);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, throws_when_unable_to_obtain_gid)
{
    bool uid_invoked{false};
    bool gid_invoked{false};
    auto request_exec = [this, &uid_invoked, &gid_invoked](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("id -u") != std::string::npos)
        {
            uid_invoked = true;
        }
        else if (cmd.find("id -g") != std::string::npos)
        {
            uid_invoked = false;
            gid_invoked = true;
            exit_status_mock.return_exit_code(SSH_ERROR);
        }
        return SSH_OK;
    };
    REPLACE(ssh_channel_request_exec, request_exec);

    std::string output{"1000"};
    auto remaining = output.size();
    auto channel_read = make_channel_read_return(output, remaining, uid_invoked);
    REPLACE(ssh_channel_read_timeout, channel_read);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(gid_invoked);
}

TEST_F(SshfsMount, throws_when_gid_is_not_an_integer)
{
    bool uid_invoked{false};
    bool gid_invoked{false};
    std::string output;
    std::string::size_type remaining;
    auto request_exec = [this, &uid_invoked, &gid_invoked, &output, &remaining](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("id -u") != std::string::npos)
        {
            uid_invoked = true;
            output = "1000";
            remaining = output.size();
        }
        else if (cmd.find("id -g") != std::string::npos)
        {
            uid_invoked = false;
            gid_invoked = true;
            output = "ubuntu";
            remaining = output.size();
            exit_status_mock.return_exit_code(SSH_ERROR);
        }
        return SSH_OK;
    };
    REPLACE(ssh_channel_request_exec, request_exec);

    auto channel_read = make_channel_read_return(output, remaining, uid_invoked);
    REPLACE(ssh_channel_read_timeout, channel_read);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(gid_invoked);
}

TEST_F(SshfsMount, unblocks_when_sftpserver_exits)
{
    bool invoked{false};
    std::string output{"1000"};
    auto remaining = output.size();
    auto channel_read = make_channel_read_return(output, remaining, invoked);
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto request_exec = [&invoked, &remaining, &output](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("id -u") != std::string::npos)
        {
            invoked = true;
            // Reset for the next channel read
            remaining = output.size();
        }
        else if (cmd.find("id -g") != std::string::npos)
        {
            // Reset for the next channel read
            remaining = output.size();
        }
        return SSH_OK;
    };
    REPLACE(ssh_channel_request_exec, request_exec);

    mpt::Signal client_message;
    auto get_client_msg = [&client_message](sftp_session) {
        client_message.wait();
        return nullptr;
    };
    REPLACE(sftp_get_client_message, get_client_msg);

    bool stopped_ok = false;
    std::thread mount([&] {
        auto sshfs_mount = make_sshfsmount(); // blocks until it asked to quit
        stopped_ok = true;
    });

    client_message.signal();

    mount.join();
    EXPECT_TRUE(stopped_ok);
}

TEST_F(SshfsMount, throws_when_unable_to_change_dir)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for("cd", invoked);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, throws_when_unable_to_get_current_dir)
{
    bool invoked{false};
    auto request_exec = make_exec_that_fails_for("pwd", invoked);
    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sshfsmount(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SshfsMount, executes_commands)
{
    CommandVector commands = {
        {"which sshfs", "/usr/bin/sshfs"},
        {"pwd", "/home/ubuntu"},
        {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=${P%/*}; done; echo $P/'",
         "/home/ubuntu/"},
        {"sudo /bin/bash -c 'cd \"/home/ubuntu/\" && mkdir -p \"target\"'", ""},
        {"id -u", "1000"},
        {"id -g", "1000"},
        {"sudo /bin/bash -c 'cd \"/home/ubuntu/\" && chown -R 1000:1000 target'", ""},
        {"id -u", "1000"},
        {"id -g", "1000"},
        {"sudo sshfs -o slave -o nonempty -o transform_symlinks -o allow_other :\"source\" \"target\"", "don't care"}};

    test_command_execution(commands, std::string("target"));
}

TEST_F(SshfsMount, works_with_absolute_paths)
{
    CommandVector commands = {
        {"sudo /bin/bash -c 'P=\"/home/ubuntu/target\"; while [ ! -d \"$P/\" ]; do P=${P%/*}; done; echo $P/'",
         "/home/ubuntu/"},
        {"id -u", "1000"},
        {"id -g", "1000"},
        {"id -u", "1000"},
        {"id -g", "1000"}};

    test_command_execution(commands, std::string("/home/ubuntu/target"));
}
