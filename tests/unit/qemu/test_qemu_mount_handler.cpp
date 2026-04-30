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

#include "tests/unit/common.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_server_reader_writer.h"
#include "tests/unit/mock_ssh_process.h"
#include "tests/unit/mock_ssh_session.h"
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/stub_ssh_key_provider.h"

#include "qemu_mount_handler.h"

#include <multipass/exceptions/ssh_exception.h>
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
    CommandOutput(const std::string& output, int exit_code = 0)
        : output{output}, exit_code{exit_code}
    {
    }

    std::string output;
    int exit_code;
};

typedef std::unordered_map<std::string, CommandOutput> CommandOutputs;

std::string command_get_existing_parent(const std::string& path)
{
    return fmt::format(
        R"(sudo /bin/bash -c 'P="{}"; while [ ! -d "$P/" ]; do P="${{P%/*}}"; done; echo $P/')",
        path);
}

std::string tag_from_target(const std::string& target)
{
    return multipass::QemuMountHandler::make_tag(target);
}

std::string command_mount(const std::string& target)
{
    return fmt::format("sudo mount -t 9p {} {} -o trans=virtio,version=9p2000.L,msize=536870912",
                       tag_from_target(target),
                       target);
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
    return fmt::format("sudo /bin/bash -c 'cd \"{}\" && chown -R {}:{} \"{}\"'",
                       parent,
                       uid,
                       gid,
                       missing.substr(0, missing.find_first_of('/')));
}

struct QemuMountHandlerTest : public ::Test
{
    QemuMountHandlerTest()
    {
        EXPECT_CALL(mock_file_ops, status)
            .WillOnce(
                Return(mp::fs::file_status{mp::fs::file_type::directory, mp::fs::perms::all}));
        EXPECT_CALL(vm, modifiable_mount_args).WillOnce(ReturnRef(mount_args));
    }

    template <bool Success>
    static void expect_ssh_aux(mpt::MockSSHSession& session,
                               const std::string& cmd,
                               std::string output)
    {
        auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();

        EXPECT_CALL(*proc, get_cmd).WillRepeatedly(ReturnRef(cmd));
        EXPECT_CALL(*proc, exit_code).WillRepeatedly(Return(Success ? 0 : 1));

        auto& out_err_expectation = Success ? EXPECT_CALL(*proc, read_std_output)
                                            : EXPECT_CALL(*proc, read_std_error);
        out_err_expectation.WillOnce(Return(std::move(output)));

        EXPECT_CALL(session, exec(HasSubstr(cmd), _)).WillOnce(Return(std::move(proc)));
    }

    static void expect_ssh_success(mpt::MockSSHSession& session,
                                   const std::string& cmd,
                                   std::string output = "")
    {
        expect_ssh_aux<true>(session, cmd, std::move(output));
    }

    static void expect_ssh_fail(mpt::MockSSHSession& session,
                                const std::string& cmd,
                                std::string output = "")
    {
        expect_ssh_aux<false>(session, cmd, std::move(output));
    }

    mpt::StubSSHKeyProvider key_provider;
    std::string default_source{"source"}, default_target{"target"};
    mp::id_mappings gid_mappings{{1, 2}}, uid_mappings{{5, 6}};
    mp::VMMount mount{default_source, gid_mappings, uid_mappings, mp::VMMount::MountType::Native};
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::debug);
    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;
    NiceMock<MockQemuVirtualMachine> vm{"my_instance"};
    mp::QemuVirtualMachine::MountArgs mount_args;
};

struct QemuMountHandlerFailCommand : public QemuMountHandlerTest,
                                     public testing::WithParamInterface<std::string>
{
    const std::string parent = "/home/ubuntu";
    const std::string missing = "target";
};
} // namespace

TEST_F(QemuMountHandlerTest, mountFailsWhenVmNotStopped)
{
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::running));
    MP_EXPECT_THROW_THAT(mp::QemuMountHandler(&vm, &key_provider, default_target, mount),
                         mp::NativeMountNeedsStoppedVMException,
                         mpt::match_what(AllOf(HasSubstr("Please stop the instance"),
                                               HasSubstr("before attempting native mounts."))));
}

TEST_F(QemuMountHandlerTest, mountFailsOnMultipleIdMappings)
{
    const mp::VMMount mount{default_source,
                            {{1, 2}, {3, 4}},
                            {{5, -1}, {6, 10}},
                            mp::VMMount::MountType::Native};
    MP_EXPECT_THROW_THAT(mp::QemuMountHandler(&vm, &key_provider, default_target, mount),
                         std::runtime_error,
                         mpt::match_what(StrEq("Only one mapping per native mount allowed.")));
}

TEST_F(QemuMountHandlerTest, mountHandlesMountArgs)
{
    {
        mp::MountHandler::UPtr mount_handler;
        EXPECT_NO_THROW(
            mount_handler =
                std::make_unique<mp::QemuMountHandler>(&vm, &key_provider, default_target, mount));
        EXPECT_EQ(mount_args.size(), 1);
        const auto uid_arg = QString("uid_map=%1:%2,")
                                 .arg(uid_mappings.front().first)
                                 .arg(uid_mappings.front().second);
        const auto gid_arg = QString{"gid_map=%1:%2,"}
                                 .arg(gid_mappings.front().first)
                                 .arg(gid_mappings.front().second);
        EXPECT_EQ(mount_args.begin()->second.second.join(' ').toStdString(),
                  fmt::format("-virtfs local,security_model=passthrough,{}{}path={},mount_tag={}",
                              uid_arg,
                              gid_arg,
                              mount.get_source_path(),
                              tag_from_target(default_target)));
    }

    EXPECT_EQ(mount_args.size(), 0);
}

TEST_F(QemuMountHandlerTest, mountLogsInit)
{
    logger_scope.mock_logger->expect_log(mpl::Level::info,
                                         fmt::format("initializing native mount {} => {} in '{}'",
                                                     mount.get_source_path(),
                                                     default_target,
                                                     vm.get_name()));
    EXPECT_NO_THROW(mp::QemuMountHandler(&vm, &key_provider, default_target, mount));
}

TEST_F(QemuMountHandlerTest, recoverFromSuspended)
{
    mount_args[tag_from_target(default_target)] = {};
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::suspended));
    logger_scope.mock_logger->expect_log(
        mpl::Level::info,
        fmt::format("Found native mount {} => {} in '{}' while suspended",
                    mount.get_source_path(),
                    default_target,
                    vm.get_name()));
    EXPECT_NO_THROW(mp::QemuMountHandler(&vm, &key_provider, default_target, mount));
}

TEST_F(QemuMountHandlerTest, startSuccessStopSuccess)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();

    expect_ssh_success(*session, "echo $PWD/target", "/home/ubuntu/target");
    expect_ssh_success(*session,
                       "P=\"/home/ubuntu/target\"",
                       "/home/ubuntu/target"); // Simulate existing path
    expect_ssh_success(*session, "mount -t 9p", "");

    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));
    EXPECT_NO_THROW(handler.deactivate());
}

TEST_F(QemuMountHandlerTest, stopFailNonforceThrows)
{
    auto error = "device is busy";
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();

    expect_ssh_success(*session, "echo $PWD/target", "/home/ubuntu/target");
    expect_ssh_success(*session, "P=\"/home/ubuntu/target\"", "/home/ubuntu/target");
    expect_ssh_success(*session, "mount -t 9p", "");

    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));

    EXPECT_CALL(vm, ssh_exec(command_umount(default_target), false))
        .WillOnce(Throw(mp::SSHExecFailure(error, 1))) // direct deactivate call
        .WillRepeatedly(Return(""));                   // d'tor
    MP_EXPECT_THROW_THAT(handler.deactivate(), std::runtime_error, mpt::match_what(StrEq(error)));
}

TEST_F(QemuMountHandlerTest, stopFailForceLogs)
{
    auto error = "device is busy";
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();

    expect_ssh_success(*session, "echo $PWD/target", "/home/ubuntu/target");
    expect_ssh_success(*session, "P=\"/home/ubuntu/target\"", "/home/ubuntu/target");
    expect_ssh_success(*session, "mount -t 9p", "");

    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));

    EXPECT_CALL(vm, ssh_exec(command_umount(default_target), false))
        .WillOnce(Throw(mp::SSHExecFailure(error, 1)));
    EXPECT_CALL(*logger_scope.mock_logger, log).WillRepeatedly(Return());
    logger_scope.mock_logger->expect_log(
        mpl::Level::warning,
        fmt::format("Failed to gracefully stop mount \"{}\" in instance '{}': {}",
                    default_target,
                    vm.get_name(),
                    error));

    EXPECT_NO_THROW(handler.deactivate(/*force=*/true));
}

TEST_F(QemuMountHandlerTest, targetDirectoryMissing)
{
    const std::string parent = "/home/ubuntu";
    const std::string missing = "target";

    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();

    expect_ssh_success(*session, "echo $PWD/target", "/home/ubuntu/target");
    expect_ssh_success(*session, "P=\"/home/ubuntu/target\"",
                       parent); // Only parent exists
    expect_ssh_success(*session, "id -u", "1000");
    expect_ssh_success(*session, "id -g", "1000");
    expect_ssh_success(*session, "mkdir", "");
    expect_ssh_success(*session, "chown", "");
    expect_ssh_success(*session, "mount -t 9p", "");

    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    EXPECT_NO_THROW(handler.activate(&server));
}

INSTANTIATE_TEST_SUITE_P(QemuMountHandlerFailCommand,
                         QemuMountHandlerFailCommand,
                         testing::Values(command_mkdir("/home/ubuntu", "target"),
                                         command_chown("/home/ubuntu", "target", 1000, 1000),
                                         "id -u",
                                         "id -g",
                                         command_mount("target"),
                                         command_get_existing_parent("/home/ubuntu/target")));

TEST_P(QemuMountHandlerFailCommand, throwOnFail)
{
    const auto cmd = GetParam();
    const auto error = "failed: " + cmd;

    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();

    // get_resolved_target
    expect_ssh_success(*session, "echo $PWD/target", "/home/ubuntu/target");

    struct Step
    {
        std::string full_cmd;
        std::string pattern;
        std::string ok_output;
    };
    const std::vector<Step> sequence = {
        {command_get_existing_parent("/home/ubuntu/target"), "P=\"/home/ubuntu/target\"", parent},
        {"id -u", "id -u", "1000"},
        {"id -g", "id -g", "1000"},
        {command_mkdir(parent, missing), "mkdir", ""},
        {command_chown(parent, missing, 1000, 1000), "chown", ""},
        {command_mount("target"), "mount -t 9p", ""},
    };

    for (const auto& [full_cmd, pattern, ok_output] : sequence)
    {
        if (cmd == full_cmd)
        {
            expect_ssh_fail(*session, pattern, error);
            break;
        }
        expect_ssh_success(*session, pattern, ok_output);
    }

    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    mp::QemuMountHandler handler{&vm, &key_provider, default_target, mount};
    MP_EXPECT_THROW_THAT(handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(StrEq(error)));
}
