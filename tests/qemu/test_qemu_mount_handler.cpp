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

#include "tests/common.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/mock_server_reader_writer.h"
#include "tests/mock_ssh_process_exit_status.h"
#include "tests/mock_ssh_test_fixture.h"
#include "tests/mock_virtual_machine.h"
#include "tests/stub_ssh_key_provider.h"

#include "qemu_mount_handler.h"

#include <multipass/utils.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct MockQemuVirtualMachine : mpt::MockVirtualMachineT<mp::QemuVirtualMachine>
{
    explicit MockQemuVirtualMachine(const std::string& name)
        : mpt::MockVirtualMachineT<mp::QemuVirtualMachine>{name, mpt::StubSSHKeyProvider{}}
    {
    }

    MOCK_METHOD(mp::QemuVirtualMachine::MountArgs&, modifiable_mount_args, (), (override));
};

struct CommandOutput
{
    CommandOutput(const std::string& output, int exit_code = 0) : output{output}, exit_code{exit_code}
    {
    }

    std::string output;
    int exit_code;
};

typedef std::unordered_map<std::string, CommandOutput> CommandOutputs;

std::string command_get_existing_parent(const std::string& path)
{
    return fmt::format(R"(sudo /bin/bash -c 'P="{}"; while [ ! -d "$P/" ]; do P="${{P%/*}}"; done; echo $P/')", path);
}

std::string tag_from_target(const std::string& target)
{
    return mp::utils::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
}

std::string command_mount(const std::string& target)
{
    return fmt::format("sudo mount -t 9p {} {} -o trans=virtio,version=9p2000.L,msize=536870912",
                       tag_from_target(target), target);
}

std::string command_umount(const std::string& target)
{
    return fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target);
}

std::string command_mkdir(const std::string& parent, const std::string& missing)
{
    return fmt::format("sudo /bin/bash -c 'cd \"{}\" && mkdir -p \"{}\"'", parent, missing);
}

std::string command_chown(const std::string& parent, const std::string& missing, int uid, int gid)
{
    return fmt::format("sudo /bin/bash -c 'cd \"{}\" && chown -R {}:{} \"{}\"'", parent, uid, gid,
                       missing.substr(0, missing.find_first_of('/')));
}

std::string command_findmnt(const std::string& target)
{
    return fmt::format("findmnt --type 9p | grep '{} {}'", target, tag_from_target(target));
}

struct QemuMountHandlerTest : public ::Test
{
    QemuMountHandlerTest()
    {
        EXPECT_CALL(mock_file_ops, status)
            .WillOnce(Return(mp::fs::file_status{mp::fs::file_type::directory, mp::fs::perms::all}));
        EXPECT_CALL(vm, modifiable_mount_args).WillOnce(ReturnRef(mount_args));
    }

    // the returned lambda will modify `output` so that it can be used to mock ssh_channel_read_timeout
    auto mocked_ssh_channel_request_exec(std::string& output)
    {
        return [&](ssh_channel, const char* command) {
            if (const auto it = command_outputs.find(command); it != command_outputs.end())
            {
                output = it->second.output;
                exit_status_mock.set_exit_status(it->second.exit_code);
            }
            else
            {
                ADD_FAILURE() << "unexpected command: " << command;
            }

            return SSH_OK;
        };
    }

    static auto mocked_ssh_channel_read_timeout(const std::string& output)
    {
        return [&, copied = 0u](auto, void* dest, uint32_t count, auto...) mutable {
            auto n = std::min(static_cast<std::string::size_type>(count), output.size() - copied);
            std::copy_n(output.begin() + copied, n, static_cast<char*>(dest));
            n ? copied += n : copied = 0;
            return n;
        };
    }

    mpt::StubSSHKeyProvider key_provider;
    std::string default_source{"source"}, default_target{"target"};
    mp::id_mappings gid_mappings{{1, 2}}, uid_mappings{{5, 6}};
    mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Native};
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::debug);
    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mpt::ExitStatusMock exit_status_mock;
    NiceMock<MockQemuVirtualMachine> vm{"my_instance"};
    mp::QemuVirtualMachine::MountArgs mount_args;
    CommandOutputs command_outputs{
        {"echo $PWD/target", {"/home/ubuntu/target"}},
        {command_get_existing_parent("/home/ubuntu/target"), {"/home/ubuntu/target"}},
        {"id -u", {"1000"}},
        {"id -g", {"1000"}},
        {command_mount(default_target), {""}},
        {command_umount(default_target), {""}},
        {command_findmnt(default_target), {""}},
    };
};

struct QemuMountHandlerFailCommand : public QemuMountHandlerTest, public testing::WithParamInterface<std::string>
{
    const std::string parent = "/home/ubuntu";
    const std::string missing = "target";

    QemuMountHandlerFailCommand()
    {
        command_outputs.at(command_get_existing_parent("/home/ubuntu/target")) = parent;
        command_outputs.insert({command_mkdir(parent, missing), {""}});
        command_outputs.insert({command_chown(parent, missing, 1000, 1000), {""}});
    }
};
} // namespace

TEST_F(QemuMountHandlerTest, mount_fails_when_vm_not_stopped)
{
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::running));
    MP_EXPECT_THROW_THAT(
        mp::QemuMountHandler(&vm, &key_provider, default_target, mount), mp::NativeMountNeedsStoppedVMException,
        mpt::match_what(AllOf(HasSubstr("Please stop the instance"), HasSubstr("before attempting native mounts."))));
}

TEST_F(QemuMountHandlerTest, mount_fails_on_multiple_id_mappings)
{
    const mp::VMMount mount{default_source, {{1, 2}, {3, 4}}, {{5, -1}, {6, 10}}, mp::VMMount::MountType::Native};
    MP_EXPECT_THROW_THAT(mp::QemuMountHandler(&vm, &key_provider, default_target, mount), std::runtime_error,
                         mpt::match_what(StrEq("Only one mapping per native mount allowed.")));
}

TEST_F(QemuMountHandlerTest, mount_handles_mount_args)
{
    {
        mp::MountHandler::UPtr mount_handler;
        EXPECT_NO_THROW(mount_handler =
                            std::make_unique<mp::QemuMountHandler>(&vm, &key_provider, default_target, mount));
        EXPECT_EQ(mount_args.size(), 1);
        const auto uid_arg = QString("uid_map=%1:%2,").arg(uid_mappings.front().first).arg(uid_mappings.front().second);
        const auto gid_arg = QString{"gid_map=%1:%2,"}.arg(gid_mappings.front().first).arg(gid_mappings.front().second);
        EXPECT_EQ(mount_args.begin()->second.second.join(' ').toStdString(),
                  fmt::format("-virtfs local,security_model=passthrough,{}{}path={},mount_tag={}",
                              uid_arg,
                              gid_arg,
                              mount.get_source_path(),
                              tag_from_target(default_target)));
    }

    EXPECT_EQ(mount_args.size(), 0);
}

TEST_F(QemuMountHandlerTest, mount_logs_init)
{
    logger_scope.mock_logger->expect_log(
        mpl::Level::info,
        fmt::format("initializing native mount {} => {} in '{}'", mount.get_source_path(), default_target, vm.vm_name));
    EXPECT_NO_THROW(mp::QemuMountHandler(&vm, &key_provider, default_target, mount));
}

TEST_F(QemuMountHandlerTest, recover_from_suspended)
{
    mount_args[tag_from_target(default_target)] = {};
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::suspended));
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         fmt::format("Found native mount {} => {} in '{}' while suspended",
                                                     mount.get_source_path(),
                                                     default_target,
                                                     vm.vm_name));
    EXPECT_NO_THROW(mp::QemuMountHandler(&vm, &key_provider, default_target, mount));
}

TEST_F(QemuMountHandlerTest, start_success_stop_success)
{
    std::string ssh_command_output;
    REPLACE(ssh_channel_request_exec, mocked_ssh_channel_request_exec(ssh_command_output));
    REPLACE(ssh_channel_read_timeout, mocked_ssh_channel_read_timeout(ssh_command_output));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));
    EXPECT_NO_THROW(handler.deactivate());
}

TEST_F(QemuMountHandlerTest, stop_fail_nonforce_throws)
{
    auto error = "device is busy";
    command_outputs.at(command_umount(default_target)) = {error, 1};

    std::string ssh_command_output;
    REPLACE(ssh_channel_request_exec, mocked_ssh_channel_request_exec(ssh_command_output));
    REPLACE(ssh_channel_read_timeout, mocked_ssh_channel_read_timeout(ssh_command_output));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));
    MP_EXPECT_THROW_THAT(handler.deactivate(), std::runtime_error, mpt::match_what(StrEq(error)));
}

TEST_F(QemuMountHandlerTest, stop_fail_force_logs)
{
    auto error = "device is busy";
    command_outputs.at(command_umount(default_target)) = {error, 1};
    std::string ssh_command_output;
    REPLACE(ssh_channel_request_exec, mocked_ssh_channel_request_exec(ssh_command_output));
    REPLACE(ssh_channel_read_timeout, mocked_ssh_channel_read_timeout(ssh_command_output));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));
    EXPECT_CALL(*logger_scope.mock_logger, log).WillRepeatedly(Return());
    logger_scope.mock_logger->expect_log(
        mpl::Level::warning,
        fmt::format("Failed to gracefully stop mount \"{}\" in instance '{}': {}", default_target, vm.vm_name, error));
}

TEST_F(QemuMountHandlerTest, target_directory_missing)
{
    const std::string parent = "/home/ubuntu";
    const std::string missing = "target";

    command_outputs.at(command_get_existing_parent("/home/ubuntu/target")) = parent;
    command_outputs.insert({command_mkdir(parent, missing), {""}});
    command_outputs.insert({command_chown(parent, missing, 1000, 1000), {""}});

    std::string ssh_command_output;
    REPLACE(ssh_channel_request_exec, mocked_ssh_channel_request_exec(ssh_command_output));
    REPLACE(ssh_channel_read_timeout, mocked_ssh_channel_read_timeout(ssh_command_output));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));
}

INSTANTIATE_TEST_SUITE_P(QemuMountHandlerFailCommand, QemuMountHandlerFailCommand,
                         testing::Values(command_mkdir("/home/ubuntu", "target"),
                                         command_chown("/home/ubuntu", "target", 1000, 1000), "id -u", "id -g",
                                         command_mount("target"), command_get_existing_parent("/home/ubuntu/target")));

TEST_P(QemuMountHandlerFailCommand, throw_on_fail)
{
    const auto cmd = GetParam();
    const auto error = "failed: " + cmd;
    command_outputs.at(cmd) = {error, 1};

    std::string ssh_command_output;
    REPLACE(ssh_channel_request_exec, mocked_ssh_channel_request_exec(ssh_command_output));
    REPLACE(ssh_channel_read_timeout, mocked_ssh_channel_read_timeout(ssh_command_output));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    MP_EXPECT_THROW_THAT(handler.activate(&server), std::runtime_error, mpt::match_what(StrEq(error)));
}
