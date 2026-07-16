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
#include "tests/unit/mock_aes.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_platform.h"
#include "tests/unit/mock_server_reader_writer.h"
#include "tests/unit/mock_sftp_client.h"
#include "tests/unit/mock_sftp_utils.h"
#include "tests/unit/mock_ssh_session.h"
#include "tests/unit/mock_utils.h"
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/windows/powershell_test_helper.h"

#include "smb_mount_handler.h"
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/utils.h>
#include <multipass/vm_mount.h>

#include <QHostInfo>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mpu = multipass::utils;

using namespace testing;

namespace
{

struct MockSmbManager : public mp::SmbManager
{
    MOCK_METHOD(bool, share_exists, (const QString& share_name), (const, override));
    MOCK_METHOD(void,
                create_share,
                (const QString& share_name, const QString& source, const QString& user),
                (const, override));
    MOCK_METHOD(void, remove_share, (const QString& share_name), (const, override));
};

struct SmbMountHandlerTest : public ::Test
{
    using RunSpec = mpt::PowerShellTestHelper::RunSpec;

    SmbMountHandlerTest()
    {
        EXPECT_CALL(file_ops, status)
            .WillOnce(
                Return(mp::fs::file_status{mp::fs::file_type::directory, mp::fs::perms::all}));
        EXPECT_CALL(utils, make_dir(_, QString{"enc-keys"}, _)).WillOnce(Return("enc-keys"));
        EXPECT_CALL(platform, get_username).WillOnce(Return(username));
        ON_CALL(utils, contents_of).WillByDefault(Return("irrelevant"));
        ON_CALL(utils, make_file_with_content(_, _)).WillByDefault(Return());
        EXPECT_CALL(utils, make_uuid(std::make_optional(vm.get_name())))
            .WillOnce(Return(vm_name_uuid));
        EXPECT_CALL(utils, make_uuid(std::make_optional(target))).WillOnce(Return(target_uuid));
        EXPECT_CALL(utils, make_uuid(std::make_optional(username.toStdString())))
            .WillOnce(Return(username_uuid));
        EXPECT_CALL(logger, log).WillRepeatedly(Return());
        logger.expect_log(mpl::Level::info,
                          fmt::format("Initializing native mount {} => {} in '{}'",
                                      source,
                                      target,
                                      vm.get_name()));

        MP_DELEGATE_MOCK_CALLS_ON_BASE(utils, run_in_ssh_session, mp::Utils);
    }

    static void expect_dpkg_query(mpt::MockSSHSession& session, const std::string& status)
    {
        auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*proc, read_std_output).WillOnce(Return(status));
        EXPECT_CALL(session, exec(HasSubstr("dpkg-query"), _)).WillOnce(Return(std::move(proc)));
    }

    static void expect_ssh_success(mpt::MockSSHSession& session, const std::string& cmd)
    {
        auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*proc, exit_code).WillRepeatedly(Return(0));
        EXPECT_CALL(session, exec(HasSubstr(cmd), _)).WillOnce(Return(std::move(proc)));
    }

    static void expect_mount_path_success(mpt::MockSSHSession& session,
                                          const std::string& target_path,
                                          const std::string& credentials_path)
    {
        auto mkdir_proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*mkdir_proc, exit_code).WillRepeatedly(Return(0));
        EXPECT_CALL(session, exec(AllOf(HasSubstr("mkdir -p"), HasSubstr(target_path)), _))
            .WillOnce(Return(std::move(mkdir_proc)));

        auto mount_proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*mount_proc, exit_code).WillRepeatedly(Return(0));
        EXPECT_CALL(session,
                    exec(AllOf(HasSubstr("mount -t cifs"),
                               HasSubstr(target_path),
                               HasSubstr(credentials_path)),
                         _))
            .WillOnce(Return(std::move(mount_proc)));

        auto rm_proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*rm_proc, exit_code).WillRepeatedly(Return(0));
        EXPECT_CALL(session, exec(AllOf(HasSubstr("sudo rm"), HasSubstr(credentials_path)), _))
            .WillOnce(Return(std::move(rm_proc)));
    }

    static void expect_ssh_fail(mpt::MockSSHSession& session,
                                const std::string& cmd,
                                const std::string& error = "")
    {
        auto proc = std::make_unique<NiceMock<mpt::MockSSHProcess>>();
        EXPECT_CALL(*proc, exit_code).WillOnce(Return(1));
        if (!error.empty())
            EXPECT_CALL(*proc, read_std_error).WillOnce(Return(error));
        EXPECT_CALL(session, exec(HasSubstr(cmd), _)).WillOnce(Return(std::move(proc)));
    }

    std::function<bool(mp::MountRequest*)> set_password = [this](mp::MountRequest* request) {
        request->set_password(password);
        return true;
    };

    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;

    NiceMock<mpt::MockVirtualMachine> vm{};
    std::string source{"source"}, target{"target"};
    std::string target_uuid{"d02a0ba3-2170-46ac-9445-1943a0fe82e6"};
    std::string vm_name_uuid{"d02a0ba3-2170-46ac-9445-1943a0fe82e6"};
    mp::id_mappings gid_mappings{{1, 2}}, uid_mappings{{5, 6}};
    mp::VMMount mount{source, gid_mappings, uid_mappings, mp::VMMount::MountType::Native};

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(mpl::Level::debug);
    mpt::MockLogger& logger = *logger_scope.mock_logger;
    mpt::MockFileOps::GuardedMock mock_file_ops_guard = mpt::MockFileOps::inject();
    mpt::MockFileOps& file_ops = *mock_file_ops_guard.first;
    mpt::MockSFTPUtils::GuardedMock mock_sftp_utils_guard = mpt::MockSFTPUtils::inject();
    mpt::MockSFTPUtils& sftp_utils = *mock_sftp_utils_guard.first;
    mpt::MockUtils::GuardedMock mock_utils_guard = mpt::MockUtils::inject();
    mpt::MockUtils& utils = *mock_utils_guard.first;
    mpt::MockPlatform::GuardedMock mock_platform_guard = mpt::MockPlatform::inject();
    mpt::MockPlatform& platform = *mock_platform_guard.first;
    mpt::MockAES::GuardedMock mock_aes_guard = mpt::MockAES::inject();
    mpt::MockAES& aes = *mock_aes_guard.first;
    std::unique_ptr<mpt::MockSFTPClient> sftp_client = std::make_unique<mpt::MockSFTPClient>();
    MockSmbManager smb_manager{};

    QString username{"username"};
    std::string username_uuid{"531b4c6f-6090-4b4c-b585-760d18db05e0"};
    std::string password{"password"};
    QString local_cred_dir{"/some/path"};
    std::string smb_share_name = fmt::format("{}-{}", vm_name_uuid, target_uuid);
    std::string remote_creds_path{"/tmp/.smb_credentials"};
    std::string umount_command{
        fmt::format("if mountpoint -q {0}; then sudo umount {0}; else true; fi", target)};
};
} // namespace

TEST_F(SmbMountHandlerTest, success)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_mount_path_success(*session, target, remote_creds_path);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    EXPECT_CALL(smb_manager, share_exists).WillOnce(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    handler.activate(&server);
}

TEST_F(SmbMountHandlerTest, generateKey)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_mount_path_success(*session, target, remote_creds_path);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(false));
    logger.expect_log(mpl::Level::info, "Successfully generated new encryption key");
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    EXPECT_CALL(smb_manager, share_exists).WillOnce(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    handler.activate(&server);
}

TEST_F(SmbMountHandlerTest, installsCifs)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "not installed");
    expect_ssh_success(*session, "apt-get");
    expect_mount_path_success(*session, target, remote_creds_path);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(server,
                Write(Property(&mp::MountReply::reply_message, "Enabling support for mounting"), _))
        .WillOnce(Return(true));
    logger.expect_log(mpl::Level::info,
                      fmt::format("Installing cifs-utils in '{}'", vm.get_name()));

    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    EXPECT_CALL(smb_manager, share_exists).WillOnce(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    handler.activate(&server);
}

TEST_F(SmbMountHandlerTest, failInstallCifs)
{
    auto install_error = "error reasons";
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();

    expect_dpkg_query(*session, "not installed");
    expect_ssh_fail(*session, "apt-get", install_error);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(server,
                Write(Property(&mp::MountReply::reply_message, "Enabling support for mounting"), _))
        .WillOnce(Return(true));
    logger.expect_log(mpl::Level::info,
                      fmt::format("Installing cifs-utils in '{}'", vm.get_name()));
    logger.expect_log(
        mpl::Level::warning,
        fmt::format("Failed to install 'cifs-utils', error message: '{}'", install_error));

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(StrEq("Failed to install cifs-utils")));
}

TEST_F(SmbMountHandlerTest, requestAndReceiveCreds)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_mount_path_success(*session, target, remote_creds_path);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(""));

    EXPECT_CALL(server, Write(Property(&mp::MountReply::password_requested, true), _))
        .WillOnce(Return(true));
    EXPECT_CALL(server, Read).WillOnce(set_password);
    EXPECT_CALL(aes, encrypt).WillOnce(Return("encrypted"));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    EXPECT_CALL(smb_manager, share_exists).WillOnce(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    handler.activate(&server);
}

TEST_F(SmbMountHandlerTest, failWithoutClient)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(""));

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(static_cast<decltype(&server)>(nullptr)),
                         std::runtime_error,
                         mpt::match_what(StrEq("Cannot get password without client connection")));
}

TEST_F(SmbMountHandlerTest, failRequestCreds)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(""));

    EXPECT_CALL(server, Write(Property(&mp::MountReply::password_requested, true), _))
        .WillOnce(Return(false));

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(
        handler.activate(&server),
        std::runtime_error,
        mpt::match_what(StrEq("Cannot request password from client. Aborting...")));
}

TEST_F(SmbMountHandlerTest, failReceiveCreds)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(""));

    EXPECT_CALL(server, Write(Property(&mp::MountReply::password_requested, true), _))
        .WillOnce(Return(true));
    EXPECT_CALL(server, Read).WillOnce(Return(false));

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(StrEq("Cannot get password from client. Aborting...")));
}

TEST_F(SmbMountHandlerTest, failEmptyPassword)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(""));

    EXPECT_CALL(server, Write(Property(&mp::MountReply::password_requested, true), _))
        .WillOnce(Return(true));
    password.clear();
    EXPECT_CALL(server, Read).WillOnce(set_password);

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(StrEq("A password is required for SMB mounts.")));
}

TEST_F(SmbMountHandlerTest, failCreateSmbShare)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    const auto error = fmt::format("failed creating SMB share for \"{}\"", source);
    EXPECT_CALL(smb_manager, create_share).WillOnce(Throw(std::runtime_error{error}));

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(&server), std::runtime_error, mpt::match_what(error));
}

TEST_F(SmbMountHandlerTest, failMkdirTarget)
{
    auto mkdir_error = "error reason";
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_ssh_fail(*session, "mkdir", mkdir_error);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(fmt::format("Cannot create \"{}\" in instance '{}': {}",
                                                     target,
                                                     vm.get_name(),
                                                     mkdir_error)));
}

TEST_F(SmbMountHandlerTest, failMountCommand)
{
    auto mount_error = "error reason";
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_ssh_success(*session, "mkdir -p " + target);
    expect_ssh_fail(*session, "mount -t cifs", mount_error);
    expect_ssh_success(*session, "sudo rm " + remote_creds_path);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    EXPECT_CALL(file_ops, remove(A<QFile&>())).WillRepeatedly(Return(true));

    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    MP_EXPECT_THROW_THAT(handler.activate(&server),
                         std::runtime_error,
                         mpt::match_what(fmt::format("Error: {}", mount_error)));
}

TEST_F(SmbMountHandlerTest, failRemoveCredsFile)
{
    auto rm_error = "error reason";
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_ssh_success(*session, "mkdir -p " + target);
    expect_ssh_success(*session, "mount -t cifs");
    expect_ssh_fail(*session, "sudo rm " + remote_creds_path, rm_error);
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    logger.expect_log(
        mpl::Level::warning,
        fmt::format("Failed deleting credentials file in \'{}\': {}", vm.get_name(), rm_error));

    EXPECT_CALL(smb_manager, share_exists).WillOnce(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    EXPECT_NO_THROW(handler.activate(&server));
}

TEST_F(SmbMountHandlerTest, stopForceFailUmountCommand)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_mount_path_success(*session, target, remote_creds_path);
    auto umount_error = "error reason";
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    logger.expect_log(mpl::Level::warning,
                      fmt::format("Failed to gracefully stop mount \"{}\" in instance '{}': {}",
                                  target,
                                  vm.get_name(),
                                  umount_error));

    EXPECT_CALL(smb_manager, share_exists).WillOnce(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    handler.activate(&server);

    EXPECT_CALL(vm, ssh_exec(umount_command, false))
        .WillOnce(Throw(mp::SSHExecFailure(umount_error, 1)));
    EXPECT_NO_THROW(handler.deactivate(/*force=*/true));
}

TEST_F(SmbMountHandlerTest, stopNonForceFailUmountCommand)
{
    auto session = std::make_unique<NiceMock<mpt::MockSSHSession>>();
    expect_dpkg_query(*session, "installed");
    expect_mount_path_success(*session, target, remote_creds_path);
    auto umount_error = "error reason";
    EXPECT_CALL(vm, new_ssh_session()).WillOnce(Return(std::move(session)));

    EXPECT_CALL(file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(aes, decrypt).WillOnce(Return(fmt::format("password={}", password)));

    EXPECT_CALL(smb_manager, create_share).WillOnce(Return());

    EXPECT_CALL(*sftp_client, from_cin).WillOnce(Return());
    EXPECT_CALL(sftp_utils, make_SFTPClient).WillOnce(Return(std::move(sftp_client)));

    EXPECT_CALL(smb_manager, share_exists).WillRepeatedly(Return(true));
    EXPECT_CALL(smb_manager, remove_share).WillOnce(Return());

    mp::SmbMountHandler handler{&vm, target, mount, local_cred_dir, smb_manager};
    handler.activate(&server);

    EXPECT_CALL(vm, ssh_exec(umount_command, false))
        .WillOnce(Throw(mp::SSHExecFailure(umount_error, 1)))
        .WillRepeatedly(Return(""));
    MP_EXPECT_THROW_THAT(handler.deactivate(/*force=*/false),
                         std::runtime_error,
                         mpt::match_what(StrEq(umount_error)));
}
