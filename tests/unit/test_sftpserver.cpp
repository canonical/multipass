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
#include "disabling_macros.h"
#include "file_operations.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_platform.h"
#include "mock_recursive_dir_iterator.h"
#include "mock_ssh_process_exit_status.h"
#include "path.h"
#include "sftp_server_test_fixture.h"
#include "stub_ssh_key_provider.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <src/sshfs_mount/sftp_server.h>

#include <multipass/cli/client_platform.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>

#include <algorithm>
#include <queue>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
namespace mcp = multipass::cli::platform;
namespace fs = std::filesystem;

using namespace testing;

using StringUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;

namespace
{
constexpr uint8_t SFTP_BAD_MESSAGE{255u};
static const int default_uid = mcp::getuid();
static const int default_gid = mcp::getgid();

struct SftpServer : public mp::test::SftpServerTest
{
    mp::SftpServer make_sftpserver()
    {
        return make_sftpserver("");
    }

    mp::SftpServer make_sftpserver(
        const std::string& path,
        const mp::id_mappings& uid_mappings = {{default_uid, mp::default_id}},
        const mp::id_mappings& gid_mappings = {{default_gid, mp::default_id}},
        const std::string& target = {})
    {
        mp::SSHSession session{"a", 42, "ubuntu", key_provider};
        return {std::move(session),
                path,
                target.empty() ? path : target,
                gid_mappings,
                uid_mappings,
                default_uid,
                default_gid,
                "sshfs"};
    }

    auto make_msg(uint8_t type = SFTP_BAD_MESSAGE)
    {
        auto msg = std::make_unique<sftp_client_message_struct>();
        msg->type = type;
        messages.push(msg.get());
        return msg;
    }

    auto make_msg_handler()
    {
        auto msg_handler = [this](auto...) -> sftp_client_message {
            if (messages.empty())
                return nullptr;
            auto msg = messages.front();
            messages.pop();
            return msg;
        };
        return msg_handler;
    }

    auto make_reply_status(sftp_client_message expected_msg,
                           uint32_t expected_status,
                           int& num_calls)
    {
        auto reply_status = [expected_msg, expected_status, &num_calls](sftp_client_message msg,
                                                                        uint32_t status,
                                                                        const char*) {
            EXPECT_THAT(msg, Eq(expected_msg));
            EXPECT_THAT(status, Eq(expected_status));
            ++num_calls;
            return SSH_OK;
        };
        return reply_status;
    }

    const mpt::StubSSHKeyProvider key_provider;
    mpt::ExitStatusMock exit_status_mock;
    std::queue<sftp_client_message> messages;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

struct MessageAndReply
{
    MessageAndReply(uint8_t type, uint32_t reply_status)
        : message_type{type}, reply_status_type{reply_status}
    {
    }
    uint8_t message_type;
    uint32_t reply_status_type;
};

struct PathTestData
{
    PathTestData(uint8_t type,
                 std::string input_path,
                 std::string expected_path,
                 uint32_t reply_status)

        : message_type{type},
          input_path{input_path},
          expected_path{expected_path},
          expected_status{reply_status}
    {
    }
    uint8_t message_type;
    std::string input_path;
    std::string expected_path;
    uint32_t expected_status; // SSH_FX_OK, SSH_FX_PERMISSION_DENIED, etc.
};

struct WhenInvalidMessageReceived : public SftpServer,
                                    public ::testing::WithParamInterface<MessageAndReply>
{
};

struct PathValidation : public SftpServer, public ::testing::WithParamInterface<PathTestData>
{
};

struct HostToGuestTranslation : public SftpServer,
                                public ::testing::WithParamInterface<PathTestData>
{
};

struct AbsolutePath : public SftpServer, public ::testing::WithParamInterface<PathTestData>
{
};

struct Stat : public SftpServer, public ::testing::WithParamInterface<uint8_t>
{
};

struct WhenInInvalidDir : public SftpServer, public ::testing::WithParamInterface<uint8_t>
{
};

std::string name_for_message(uint8_t message_type)
{
    switch (message_type)
    {
    case SFTP_BAD_MESSAGE:
        return "SFTP_BAD_MESSAGE";
    case SFTP_CLOSE:
        return "SFTP_CLOSE";
    case SFTP_READ:
        return "SFTP_READ";
    case SFTP_FSETSTAT:
        return "SFTP_FSETSTAT";
    case SFTP_SETSTAT:
        return "SFTP_SETSTAT";
    case SFTP_FSTAT:
        return "SFTP_FSTAT";
    case SFTP_READDIR:
        return "SFTP_READDIR";
    case SFTP_WRITE:
        return "SFTP_WRITE";
    case SFTP_OPENDIR:
        return "SFTP_OPENDIR";
    case SFTP_STAT:
        return "SFTP_STAT";
    case SFTP_LSTAT:
        return "SFTP_LSTAT";
    case SFTP_READLINK:
        return "SFTP_READLINK";
    case SFTP_SYMLINK:
        return "SFTP_SYMLINK";
    case SFTP_RENAME:
        return "SFTP_RENAME";
    case SFTP_EXTENDED:
        return "SFTP_EXTENDED";
    case SFTP_MKDIR:
        return "SFTP_MKDIR";
    case SFTP_RMDIR:
        return "SFTP_RMDIR";
    case SFTP_OPEN:
        return "SFTP_OPEN";
    case SFTP_REALPATH:
        return "SFTP_REALPATH";
    case SFTP_REMOVE:
        return "SFTP_REMOVE";
    default:
        return "Unknown";
    }
}

std::string name_for_status(uint32_t status_type)
{
    switch (status_type)
    {
    case SSH_FX_OK:
        return "SSH_FX_OK";
    case SSH_FX_OP_UNSUPPORTED:
        return "SSH_FX_OP_UNSUPPORTED";
    case SSH_FX_BAD_MESSAGE:
        return "SSH_FX_BAD_MESSAGE";
    case SSH_FX_NO_SUCH_FILE:
        return "SSH_FX_NO_SUCH_FILE";
    case SSH_FX_FAILURE:
        return "SSH_FX_FAILURE";
    default:
        return "Unknown";
    }
}

std::string name_for_path(const std::string& path)
{
    auto result{fs::path(path).generic_string()};
    std::replace(result.begin(), result.end(), '.', 'd');
    std::replace(result.begin(), result.end(), '/', '_');
    std::replace(result.begin(), result.end(), '-', '_');
    return result;
}

std::string string_for_pathdata(const ::testing::TestParamInfo<PathTestData>& info)
{
    return fmt::format("message_{}_input_{}_expected_{}_replies_{}",
                       name_for_message(info.param.message_type),
                       name_for_path(info.param.input_path),
                       name_for_path(info.param.expected_path),
                       name_for_status(info.param.expected_status));
}

std::string string_for_param(const ::testing::TestParamInfo<MessageAndReply>& info)
{
    return fmt::format("message_{}_replies_{}",
                       name_for_message(info.param.message_type),
                       name_for_status(info.param.reply_status_type));
}

std::string string_for_message(const ::testing::TestParamInfo<uint8_t>& info)
{
    return fmt::format("message_{}", name_for_message(info.param));
}

auto name_as_char_array(const std::string& name)
{
    std::vector<char> out(name.begin(), name.end());
    out.push_back('\0');
    return out;
}

auto make_data(const std::string& in)
{
    StringUPtr out{ssh_string_new(in.size()), ssh_string_free};
    ssh_string_fill(out.get(), in.data(), in.size());
    return out;
}

bool content_match(const QString& path, const std::string& data)
{
    auto content = mpt::load(path);

    const int data_size = data.size();
    if (content.size() != data_size)
        return false;

    return std::equal(data.begin(), data.end(), content.begin());
}

enum class Permission
{
    Owner,
    Group,
    Other
};
bool compare_permission(uint32_t ssh_permissions, const QFileInfo& file, Permission perm_type)
{
    uint16_t qt_perm_mask{0u}, ssh_perm_mask{0u}, qt_bitshift{0u}, ssh_bitshift{0u};

    // Comparing file permissions, sftp uses octal format: (aaabbbccc), QFileInfo uses hex format
    // (aaaa----bbbbcccc)
    switch (perm_type)
    {
    case Permission::Owner:
        qt_perm_mask = 0x7000;
        qt_bitshift = 12;
        ssh_perm_mask = 0700;
        ssh_bitshift = 6;
        break;
    case Permission::Group:
        qt_perm_mask = 0x70;
        qt_bitshift = 4;
        ssh_perm_mask = 070;
        ssh_bitshift = 3;
        break;
    case Permission::Other:
        qt_perm_mask = 0x7;
        qt_bitshift = 0;
        ssh_perm_mask = 07;
        ssh_bitshift = 0;
        break;
    }

    return ((ssh_permissions & ssh_perm_mask) >> ssh_bitshift) ==
           ((static_cast<uint32_t>(file.permissions()) & qt_perm_mask) >> qt_bitshift);
}
} // namespace

TEST_F(SftpServer, throwsWhenMessageNull)
{
    EXPECT_THROW(make_sftpserver(), mp::SSHException);
}

TEST_F(SftpServer, throwsWhenMessageNotFXPInit)
{
    auto init_msg = make_msg(SFTP_BAD_MESSAGE);
    REPLACE(sftp_get_client_message, make_msg_handler());
    EXPECT_THROW(make_sftpserver(), mp::SSHException);
}

TEST_F(SftpServer, throwsWhenFailedToInit)
{
    auto init_msg = make_msg(SSH_FXP_INIT);
    REPLACE(sftp_get_client_message, make_msg_handler());
    auto bad_reply_version = [](auto...) { return SSH_ERROR; };
    REPLACE(sftp_reply_version, bad_reply_version);
    EXPECT_THROW(make_sftpserver(), mp::SSHException);
}

TEST_F(SftpServer, throwsWhenSshfsErrorsOnStart)
{
    auto init_msg = make_msg(SSH_FXP_INIT);
    REPLACE(sftp_get_client_message, make_msg_handler());

    bool invoked{false};
    auto request_exec = [this, &invoked](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("sudo sshfs") != std::string::npos)
        {
            invoked = true;
            exit_status_mock.set_exit_status(exit_status_mock.failure_status);
        }
        return SSH_OK;
    };

    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sftpserver(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SftpServer, throwsOnSshFailureReadExit)
{
    bool invoked{false};
    auto request_exec = [this, &invoked](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("sudo sshfs") != std::string::npos)
        {
            invoked = true;
            exit_status_mock.set_ssh_rc(SSH_ERROR);
            exit_status_mock.set_no_exit();
        }

        return SSH_OK;
    };

    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sftpserver(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SftpServer, sshfsRestartsOnTimeout)
{
    // This test verifies that after a sshfs timeout, the sftp server correctly restarts. To do so,
    // it simulates failure in the first non-sftp-init message. Intended execution order :
    // exec->msg->msg->exec(not "sudo sshfs")->exec->msg->msg
    int num_calls{0};
    auto request_exec = [this, &num_calls](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("sudo sshfs") != std::string::npos)
        {
            if (++num_calls < 5)
            {
                exit_status_mock.set_ssh_rc(SSH_OK);
                exit_status_mock.set_no_exit();
            }
        }

        return SSH_OK;
    };

    REPLACE(ssh_channel_request_exec, request_exec);
    auto message{make_msg(SSH_FXP_INIT)};
    auto get_client_msg = [this, &num_calls, &message](auto...) mutable {
        exit_status_mock.set_ssh_rc(SSH_OK);
        exit_status_mock.set_exit_status(num_calls == 2 ? exit_status_mock.failure_status
                                                        : exit_status_mock.success_status);
        return ((++num_calls + 1) % 3 != 0) ? nullptr : message.get();
    };
    REPLACE(sftp_get_client_message, get_client_msg);
    auto sftp = make_sftpserver();

    sftp.run();

    EXPECT_EQ(num_calls, 6);
}

TEST_F(SftpServer, stopsAfterANullMessage)
{
    auto init_msg = make_msg(SSH_FXP_INIT);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver();

    sftp.run();
}

TEST_F(SftpServer, freesMessage)
{
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_BAD_MESSAGE);

    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver();
    sftp.run();

    msg_free.expectCalled(1).withValues(msg.get());
}

TEST_F(SftpServer, handlesRealpath)
{
    mpt::TempFile file;
    auto file_name = name_as_char_array(file.name().toStdString());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_REALPATH);
    msg->filename = file_name.data();

    bool invoked{false};
    auto reply_name = [&msg, &invoked, &file_name](sftp_client_message cmsg,
                                                   const char* name,
                                                   sftp_attributes attr) {
        EXPECT_THAT(cmsg, Eq(msg.get()));
        EXPECT_THAT(name, StrEq(file_name.data()));
        invoked = true;
        return SSH_OK;
    };
    REPLACE(sftp_reply_name, reply_name);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(file.name().toStdString());
    sftp.run();

    EXPECT_TRUE(invoked);
}

TEST_F(SftpServer, realpathFailsWhenIdsAreNotMapped)
{
    mpt::TempFile file;
    auto file_name = name_as_char_array(file.name().toStdString());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_REALPATH);
    msg->filename = file_name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(file.name().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handlesOpendir)
{
    auto dir_name =
        name_as_char_array(fs::weakly_canonical(mpt::test_data_path().toStdString()).string());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce(Return(std::make_unique<mpt::MockDirIterator>()));
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    sftp.run();
}

TEST_F(SftpServer, opendirNotExistingFails)
{
    auto dir_name =
        name_as_char_array(fs::weakly_canonical(mpt::test_data_path().toStdString()).string());
    auto init_msg = make_msg(SSH_FXP_INIT);
    const auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce([&](const mp::fs::path&, std::error_code& err) {
        err = std::make_error_code(std::errc::no_such_file_or_directory);
        return std::make_unique<mpt::MockDirIterator>();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int no_such_file_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(msg.get(), SSH_FX_NO_SUCH_FILE, no_such_file_calls));

    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    sftp.run();

    EXPECT_EQ(no_such_file_calls, 1);
}

TEST_F(SftpServer, opendirNotReadableFails)
{
    auto dir_name =
        name_as_char_array(fs::weakly_canonical(mpt::test_data_path().toStdString()).string());
    auto init_msg = make_msg(SSH_FXP_INIT);
    const auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return std::make_unique<mpt::MockDirIterator>();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());

    int perm_denied_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace),
            StrEq("sftp server"),
            AllOf(HasSubstr("Cannot read directory"),
                  HasSubstr(fs::weakly_canonical(mpt::test_data_path().toStdString()).string()))));

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, opendirNoHandleAllocatedFails)
{
    auto dir_name = name_as_char_array(mpt::test_data_path().toStdString());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce([&](const mp::fs::path&, std::error_code& err) {
        err.clear();
        return std::make_unique<mpt::MockDirIterator>();
    });
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_handle_alloc, [](auto...) { return nullptr; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    StrEq("Cannot allocate handle for opendir()")));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, opendirFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto open_dir_msg = make_msg(SFTP_OPENDIR);
    auto dir_name = name_as_char_array(temp_dir.path().toStdString());
    open_dir_msg->filename = dir_name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(open_dir_msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handlesMkdir)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto init_msg = make_msg(SSH_FXP_INIT);
    const auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    msg->attr = &attr;

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*platform, set_permissions(A<const std::filesystem::path&>(), _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    int num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_OK, num_calls));
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_TRUE(QDir{new_dir_name.data()}.exists());
    EXPECT_EQ(num_calls, 1);
}

TEST_F(SftpServer, mkdirOnExistingDirFails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    QDir dir(new_dir_name.data());
    ASSERT_TRUE(dir.mkdir(new_dir_name.data()));
    ASSERT_TRUE(dir.exists());

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("mkdir failed for"), HasSubstr(new_dir))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, mkdirSetPermissionsFails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*platform, set_permissions(_, _, _)).WillOnce(Return(false));
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    msg->attr = &attr;

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("set permissions failed for"), HasSubstr(new_dir))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, mkdirChownFailureFails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, _, _)).WillOnce(Return(-1));
    EXPECT_CALL(*mock_platform, set_permissions(_, _, _)).WillRepeatedly(Return(true));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("failed to chown"), HasSubstr(new_dir))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, mkdirFailsInDirThatsMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    auto init_msg = make_msg(SSH_FXP_INIT);
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    const auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    msg->attr = &attr;

    int perm_denied_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls));
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_FALSE(QDir{new_dir_name.data()}.exists());
    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handlesRmdir)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    QDir dir(new_dir_name.data());
    ASSERT_TRUE(dir.mkdir(new_dir_name.data()));
    ASSERT_TRUE(dir.exists());

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_FALSE(dir.exists());
    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, rmdirNonExistingFails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("rmdir failed for"), HasSubstr(new_dir))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, rmdirUnableToRemoveFails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, remove(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("rmdir failed for"), HasSubstr(new_dir))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, rmdirFailsToRemoveDirThatsMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    QDir dir(new_dir_name.data());
    ASSERT_TRUE(dir.mkdir(new_dir_name.data()));
    ASSERT_TRUE(dir.exists());

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_TRUE(dir.exists());
    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handlesReadlink)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(MP_PLATFORM.symlink(file_name.toStdString().c_str(),
                                    link_name.toStdString().c_str(),
                                    QFileInfo(file_name).isDir()));
    ASSERT_TRUE(QFile::exists(link_name));
    ASSERT_TRUE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_READLINK);
    auto name = name_as_char_array(link_name.toStdString());
    msg->filename = name.data();

    int num_calls{0};
    auto names_add = [&num_calls, &msg, &file_name](sftp_client_message reply_msg,
                                                    const char* file,
                                                    const char*,
                                                    sftp_attributes) {
        EXPECT_THAT(reply_msg, Eq(msg.get()));
        EXPECT_THAT(file, StrEq(file_name.toStdString()));
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_names_add, names_add);
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, readlinkFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(MP_PLATFORM.symlink(file_name.toStdString().c_str(),
                                    link_name.toStdString().c_str(),
                                    QFileInfo(file_name).isDir()));
    ASSERT_TRUE(QFile::exists(link_name));
    ASSERT_TRUE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_READLINK);
    auto name = name_as_char_array(link_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handlesSymlink)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SYMLINK);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
    msg->attr->uid = 1000;
    msg->attr->gid = 1000;

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo info(link_name);
    EXPECT_TRUE(QFile::exists(link_name));
    EXPECT_TRUE(info.isSymLink());
    EXPECT_THAT(info.symLinkTarget(), Eq(file_name));
}

TEST_F(SftpServer, symlinkInInvalidDirFails)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SYMLINK);
    auto target = name_as_char_array("bar");
    msg->filename = target.data();

    auto invalid_link = name_as_char_array("/foo/baz");
    REPLACE(sftp_client_message_get_data, [&invalid_link](auto...) { return invalid_link.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, brokenSymlinkDoesNotFail)
{
    mpt::TempDir temp_dir;
    auto missing_file_name = temp_dir.path() + "/test-file";
    auto broken_link_name = temp_dir.path() + "/test-link";

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SYMLINK);
    auto broken_target = name_as_char_array(missing_file_name.toStdString());
    msg->filename = broken_target.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
    msg->attr->uid = 1000;
    msg->attr->gid = 1000;

    auto broken_link = name_as_char_array(broken_link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&broken_link](auto...) { return broken_link.data(); });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo info(broken_link_name);
    EXPECT_TRUE(info.isSymLink());
    EXPECT_FALSE(QFile::exists(info.symLinkTarget()));
    EXPECT_FALSE(QFile::exists(missing_file_name));
}

TEST_F(SftpServer, symlinkFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SYMLINK);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, symlink(_, _, _)).WillOnce(Return(false));

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("failure creating symlink from"),
                          HasSubstr(file_name.toStdString()),
                          HasSubstr(link_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, symlinkFailsWhenMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SYMLINK);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
    msg->attr->uid = 1000;
    msg->attr->gid = 1000;

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    ASSERT_THAT(perm_denied_num_calls, Eq(1));

    QFileInfo info(link_name);
    EXPECT_FALSE(QFile::exists(link_name));
    EXPECT_FALSE(info.isSymLink());
}

TEST_F(SftpServer, handlesRename)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_TRUE(QFile::exists(new_name));
    EXPECT_FALSE(QFile::exists(old_name));
}

TEST_F(SftpServer, renameCannotRemoveTargetFails)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);
    mpt::make_file_with_content(new_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, remove(_)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>()))
        .WillRepeatedly([](const QFileInfo& file) { return file.exists(); });
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("cannot remove"),
                          HasSubstr(new_name.toStdString()),
                          HasSubstr("for renaming"))));
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, renameFailureFails)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, rename(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>()))
        .WillRepeatedly([](const QFileInfo& file) { return file.exists(); });
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("failed renaming"),
                          HasSubstr(old_name.toStdString()),
                          HasSubstr(new_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, renameInvalidTargetFails)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto invalid_target = name_as_char_array("/foo/bar");
    mpt::make_file_with_content(old_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    REPLACE(sftp_client_message_get_data,
            [&invalid_target](auto...) { return invalid_target.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, renameFailsWhenSourceFileIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace), StrEq("sftp server"), HasSubstr(old_name.toStdString())));

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_TRUE(QFile::exists(old_name));
    EXPECT_FALSE(QFile::exists(new_name));
}

TEST_F(SftpServer, renameFailsWhenTargetFileIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);
    mpt::make_file_with_content(new_name);

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, ownerId(_))
        .WillOnce([](const QFileInfo& file) { return file.ownerId(); })
        .WillOnce([](const QFileInfo& file) { return file.ownerId() + 1; });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillOnce([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>()))
        .WillRepeatedly([](const QFileInfo& file) { return file.exists(); });
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace), StrEq("sftp server"), HasSubstr(new_name.toStdString())));

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_TRUE(QFile::exists(old_name));
    EXPECT_TRUE(QFile::exists(new_name));
}

TEST_F(SftpServer, handlesRemove)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_REMOVE);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_FALSE(QFile::exists(file_name));
}

TEST_F(SftpServer, removeNonExistingFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_REMOVE);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("cannot remove"), HasSubstr(file_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, removeFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_REMOVE);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_TRUE(QFile::exists(file_name));
}

TEST_F(SftpServer, openInWriteModeCreatesFile)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto name = name_as_char_array(file_name.toStdString());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_WRITE | SSH_FXF_CREAT;
    msg->attr = &attr;
    msg->filename = name.data();

    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*platform, chown).WillOnce(Return(0));

    bool reply_handle_invoked{false};
    auto reply_handle = [&reply_handle_invoked](auto...) {
        reply_handle_invoked = true;
        return SSH_OK;
    };
    REPLACE(sftp_reply_handle, reply_handle);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_TRUE(reply_handle_invoked);
    EXPECT_TRUE(QFile::exists(file_name));
}

TEST_F(SftpServer, openInTruncateModeTruncatesFile)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_WRITE | SSH_FXF_TRUNC;
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    bool reply_handle_invoked{false};
    auto reply_handle = [&reply_handle_invoked](auto...) {
        reply_handle_invoked = true;
        return SSH_OK;
    };
    REPLACE(sftp_reply_handle, reply_handle);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    QFile file(file_name);
    ASSERT_TRUE(reply_handle_invoked);
    EXPECT_EQ(file.size(), 0);
}

TEST_F(SftpServer, openUnableToOpenFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto name = name_as_char_array(file_name.toStdString());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_READ | SSH_FXF_WRITE | SSH_FXF_APPEND | SSH_FXF_EXCL;
    msg->attr = &attr;
    msg->filename = name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, symlink_status).WillOnce([](auto, std::error_code& err) {
        err.clear();
        return mp::fs::file_status{mp::fs::file_type::regular};
    });
    EXPECT_CALL(*file_ops, open_fd).WillOnce([](auto path, auto...) {
        errno = EACCES;
        return std::make_unique<mp::NamedFd>(path, -1);
    });
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("Cannot open"), HasSubstr(file_name.toStdString()))));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, openUnableToGetStatusFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto name = name_as_char_array(file_name.toStdString());
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_READ | SSH_FXF_WRITE | SSH_FXF_APPEND | SSH_FXF_EXCL;
    msg->attr = &attr;
    msg->filename = name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*file_ops, symlink_status).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return mp::fs::file_status{mp::fs::file_type::unknown};
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("Cannot get status"), HasSubstr(file_name.toStdString()))));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, openChownFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, _, _)).WillOnce(Return(-1));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_WRITE | SSH_FXF_CREAT;
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("failed to chown"), HasSubstr(file_name.toStdString()))));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, openNoHandleAllocatedFails)
{
    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject<NiceMock>();
    EXPECT_CALL(*platform, set_permissions(_, _, _)).WillRepeatedly(Return(true));

    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_WRITE | SSH_FXF_CREAT;
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_handle_alloc, [](auto...) { return nullptr; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    StrEq("Cannot allocate handle for open()")));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, openFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, openNonExistingFileFailsWhenDirIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    QFile file(file_name);
    EXPECT_FALSE(file.exists());
}

TEST_F(SftpServer, handlesReaddir)
{
    mpt::TempDir temp_dir;
    QDir dir_entry(temp_dir.path());

    auto test_dir = temp_dir.path() + "/test-dir-entry";
    ASSERT_TRUE(dir_entry.mkdir(test_dir));

    auto test_file = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(test_file);

    auto test_link = temp_dir.path() + "/test-link";
    ASSERT_TRUE(MP_PLATFORM.symlink(test_file.toStdString().c_str(),
                                    test_link.toStdString().c_str(),
                                    QFileInfo(test_file).isDir()));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto readdir_msg = make_msg(SFTP_READDIR);
    auto readdir_msg_final = make_msg(SFTP_READDIR);

    std::vector<mp::fs::path> expected_entries = {".",
                                                  "..",
                                                  "test-dir-entry",
                                                  "test-file",
                                                  "test-link"};
    auto entries_read = 0ul;

    auto directory_entry = mpt::MockDirectoryEntry{};
    EXPECT_CALL(directory_entry, path).WillRepeatedly([&]() -> const mp::fs::path& {
        return expected_entries[entries_read - 1];
    });
    EXPECT_CALL(directory_entry, is_symlink()).WillRepeatedly([&]() {
        return expected_entries[entries_read - 1] == "test-link";
    });
    auto dir_iterator = mpt::MockDirIterator{};
    EXPECT_CALL(dir_iterator, hasNext).WillRepeatedly([&] {
        return entries_read != expected_entries.size();
    });
    EXPECT_CALL(dir_iterator, next)
        .WillRepeatedly(DoAll([&] { entries_read++; }, ReturnRef(directory_entry)));

    REPLACE(sftp_handle, [&dir_iterator](auto...) { return &dir_iterator; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int eof_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(readdir_msg_final.get(), SSH_FX_EOF, eof_num_calls));

    std::vector<mp::fs::path> given_entries;
    auto reply_names_add = [&given_entries](auto, const char* file, auto, auto) {
        given_entries.push_back(file);
        return SSH_OK;
    };
    REPLACE(sftp_reply_names_add, reply_names_add);
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(eof_num_calls, 1);
    EXPECT_THAT(given_entries, ContainerEq(expected_entries));
}

TEST_F(SftpServer, handlesReaddirAttributesPreserved)
{
    mpt::TempDir temp_dir;
    QDir dir_entry(temp_dir.path());

    const auto test_file_name = "test-file";
    auto test_file = temp_dir.path() + "/" + test_file_name;
    mpt::make_file_with_content(test_file, "some content for the file to give it non-zero size");

    QFileDevice::Permissions expected_permissions = QFileDevice::WriteOwner |
                                                    QFileDevice::ExeGroup | QFileDevice::ReadOther;
    QFile::setPermissions(test_file, expected_permissions);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto readdir_msg = make_msg(SFTP_READDIR);
    auto readdir_msg_final = make_msg(SFTP_READDIR);

    const auto temp_dir_path = mp::fs::path{temp_dir.path().toStdString()};
    std::vector<mp::fs::path> expected_entries = {temp_dir_path / ".",
                                                  temp_dir_path / "..",
                                                  temp_dir_path / "test-file"};
    auto entries_read = 0ul;

    auto directory_entry = mpt::MockDirectoryEntry{};
    EXPECT_CALL(directory_entry, path).WillRepeatedly([&]() -> const mp::fs::path& {
        return expected_entries[entries_read - 1];
    });
    auto dir_iterator = mpt::MockDirIterator{};
    EXPECT_CALL(dir_iterator, hasNext).WillRepeatedly([&] {
        return entries_read != expected_entries.size();
    });
    EXPECT_CALL(dir_iterator, next)
        .WillRepeatedly(DoAll([&] { entries_read++; }, ReturnRef(directory_entry)));

    REPLACE(sftp_handle, [&dir_iterator](auto...) { return &dir_iterator; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int eof_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(readdir_msg_final.get(), SSH_FX_EOF, eof_num_calls));

    sftp_attributes_struct test_file_attrs{};
    auto get_test_file_attributes = [&](auto, const char* file, auto, sftp_attributes attr) {
        if (strcmp(file, test_file_name) == 0)
        {
            test_file_attrs = *attr;
        }
        return SSH_OK;
    };
    REPLACE(sftp_reply_names_add, get_test_file_attributes);
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(eof_num_calls, 1);

    QFileInfo test_file_info(test_file);
    EXPECT_EQ(test_file_attrs.size, (uint64_t)test_file_info.size());
    EXPECT_EQ(test_file_attrs.gid, test_file_info.groupId());
    EXPECT_EQ(test_file_attrs.uid, test_file_info.ownerId());
    EXPECT_EQ(
        test_file_attrs.mtime,
        (uint32_t)test_file_info.lastModified().toSecsSinceEpoch()); // atime64 is zero, expected?

    EXPECT_TRUE(compare_permission(test_file_attrs.permissions, test_file_info, Permission::Owner));
    EXPECT_TRUE(compare_permission(test_file_attrs.permissions, test_file_info, Permission::Group));
    EXPECT_TRUE(compare_permission(test_file_attrs.permissions, test_file_info, Permission::Other));
}

TEST_F(SftpServer, handlesClose)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto open_dir_msg = make_msg(SFTP_OPENDIR);
    auto dir_name = name_as_char_array(temp_dir.path().toStdString());
    open_dir_msg->filename = dir_name.data();

    auto close_msg = make_msg(SFTP_CLOSE);

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return ssh_string_new(4);
    };

    int ok_num_calls{0};
    auto reply_status = make_reply_status(close_msg.get(), SSH_FX_OK, ok_num_calls);

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_remove, [](auto...) {});

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(ok_num_calls, Eq(1));
}

TEST_F(SftpServer, handlesFstat)
{
    mpt::TempDir temp_dir;
    const auto content = std::string{"whatever just some content bla bla"};
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name, content);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto open_msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    open_msg->filename = name.data();
    open_msg->flags |= SSH_FXF_READ;

    auto fstat_msg = make_msg(SFTP_FSTAT);

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return ssh_string_new(4);
    };

    int num_calls{0};
    auto reply_attr =
        [&num_calls, &fstat_msg, expected_size = content.size()](sftp_client_message reply_msg,
                                                                 sftp_attributes attr) {
            EXPECT_THAT(reply_msg, Eq(fstat_msg.get()));
            EXPECT_THAT(attr->size, Eq(expected_size));
            ++num_calls;
            return SSH_OK;
        };

    REPLACE(sftp_reply_attr, reply_attr);
    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, handlesFsetstat)
{
    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject<NiceMock>();
    EXPECT_CALL(*platform, set_permissions(_, _, _)).WillRepeatedly(Return(true));

    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto open_msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    const int expected_size = 7777;
    attr.size = expected_size;
    attr.flags = SSH_FILEXFER_ATTR_SIZE;
    attr.permissions = 0777;

    open_msg->filename = name.data();
    open_msg->attr = &attr;
    open_msg->flags |= SSH_FXF_WRITE | SSH_FXF_TRUNC | SSH_FXF_CREAT;

    auto fsetstat_msg = make_msg(SFTP_FSETSTAT);
    fsetstat_msg->attr = &attr;

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return ssh_string_new(4);
    };

    int num_calls{0};
    auto reply_status = make_reply_status(fsetstat_msg.get(), SSH_FX_OK, num_calls);

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    QFile file(file_name);
    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_TRUE(file.exists());
    EXPECT_THAT(file.size(), Eq(expected_size));
}

TEST_F(SftpServer, handlesSetstat)
{
    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject<NiceMock>();
    EXPECT_CALL(*platform, set_permissions(_, _, _)).WillRepeatedly(Return(true));

    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    const int expected_size = 7777;
    attr.size = expected_size;
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = 0777;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    QFile file(file_name);
    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_THAT(file.size(), Eq(expected_size));
}

TEST_F(SftpServer, setstatCorrectlyModifiesFileTimestamp)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    QFileInfo orig_info{file_name};
    auto original_time = orig_info.lastModified().toSecsSinceEpoch();

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    attr.mtime = static_cast<uint32_t>(original_time + 1);
    attr.flags = SSH_FILEXFER_ATTR_ACMODTIME;

    msg->filename = name.data();
    msg->attr = &attr;

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo modified_info{file_name};
    auto new_time = modified_info.lastModified().toSecsSinceEpoch();
    EXPECT_EQ(new_time, original_time + 1);
}

TEST_F(SftpServer, setstatResizeFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    const int expected_size = 7777;
    attr.size = expected_size;
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = 0777;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, resize(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>()))
        .WillRepeatedly([](const QFileInfo& file) { return file.exists(); });
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("cannot resize"), HasSubstr(file_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstatSetPermissionsFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    attr.size = 7777;
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = 0777;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    const auto [platform, mock_platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*file_ops, resize).WillOnce(Return(true));
    EXPECT_CALL(*platform, set_permissions(_, _, _)).WillOnce(Return(false));
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.ownerId();
    });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) {
        return file.groupId();
    });
    EXPECT_CALL(*file_ops, exists(A<const QFileInfo&>())).WillRepeatedly([](const QFileInfo& file) {
        return file.exists();
    });
    EXPECT_CALL(*file_ops, weakly_canonical).WillRepeatedly([](const fs::path& path) {
        return fs::weakly_canonical(path);
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace),
            StrEq("sftp server"),
            AllOf(HasSubstr("set permissions failed for"), HasSubstr(file_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstatChownFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    const int expected_size = 7777;
    attr.size = expected_size;
    attr.flags = SSH_FILEXFER_ATTR_UIDGID;
    attr.permissions = 0777;
    attr.uid = 1001;
    attr.gid = 1001;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, _, _)).WillOnce(Return(-1));

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(),
                                {{default_uid, -1}, {1001, 1001}},
                                {{default_gid, -1}, {1001, 1001}});

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace),
            StrEq("sftp server"),
            AllOf(HasSubstr("cannot set ownership for"), HasSubstr(file_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstatUtimeFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    const int expected_size = 7777;
    attr.size = expected_size;
    attr.flags = SSH_FILEXFER_ATTR_ACMODTIME;
    attr.permissions = 0777;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, utime(_, _, _)).WillOnce(Return(-1));

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("cannot set modification date for"),
                          HasSubstr(file_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstatFailsWhenMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);
    QFile file(file_name);
    auto file_size = file.size();

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    attr.size = 777;
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = 0777;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_EQ(file.size(), file_size);
}

TEST_F(SftpServer, setstatChownFailsWhenNewIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    attr.flags = SSH_FILEXFER_ATTR_UIDGID;
    attr.uid = 1001;
    attr.gid = 1001;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, chown(_, _, _)).Times(0);

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handlesWrites)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto write_msg1 = make_msg(SFTP_WRITE);
    auto data1 = make_data("The answer is ");
    write_msg1->data = data1.get();
    write_msg1->offset = 0;

    auto write_msg2 = make_msg(SFTP_WRITE);
    auto data2 = make_data("always 42");
    write_msg2->data = data2.get();
    write_msg2->offset = ssh_string_len(data1.get());

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    std::stringstream stream;

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek).WillRepeatedly(Return(true));
    EXPECT_CALL(*file_ops, write(fd, _, _))
        .WillRepeatedly([&stream](int, const void* buf, size_t nbytes) {
            stream.write((const char*)buf, nbytes);
            return nbytes;
        });

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = [&num_calls](auto, uint32_t status, auto) {
        EXPECT_TRUE(status == SSH_FX_OK);
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_EQ(num_calls, 2);
    EXPECT_EQ(stream.str(), "The answer is always 42");
}

TEST_F(SftpServer, writeCannotSeekFails)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto write_msg = make_msg(SFTP_WRITE);
    auto data1 = make_data("The answer is ");
    write_msg->data = data1.get();
    write_msg->offset = 10;

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(-1));

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(write_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, writeFailureFails)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto write_msg = make_msg(SFTP_WRITE);
    auto data1 = make_data("The answer is ");
    write_msg->data = data1.get();
    write_msg->offset = 10;

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*file_ops, write(fd, _, _)).WillRepeatedly(Return(-1));

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(write_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, handlesReads)
{
    mpt::TempDir temp_dir;

    std::string given_data{"some text"};
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto read_msg = make_msg(SFTP_READ);
    read_msg->offset = 0;
    read_msg->len = given_data.size();

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*file_ops, read(fd, _, _))
        .WillRepeatedly([&given_data, r = 0](int, void* buf, size_t count) mutable {
            ::memcpy(buf, given_data.c_str() + r, count);
            r += count;
            return count;
        });

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_data = [&](sftp_client_message msg, const void* data, int len) {
        EXPECT_GT(len, 0);
        EXPECT_EQ(msg, read_msg.get());

        std::string data_read{reinterpret_cast<const char*>(data),
                              static_cast<std::string::size_type>(len)};
        EXPECT_EQ(data_read, given_data);
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_data, reply_data);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_EQ(num_calls, 1);
}

TEST_F(SftpServer, readCannotSeekFails)
{
    mpt::TempDir temp_dir;

    std::string given_data{"some text"};
    const int seek_pos{10};
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto read_msg = make_msg(SFTP_READ);
    read_msg->offset = seek_pos;
    read_msg->len = given_data.size();

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(-1));

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(read_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(mpl::Level::trace,
                    StrEq("sftp server"),
                    AllOf(HasSubstr(fmt::format("cannot seek to position {} in", seek_pos)),
                          HasSubstr(path.string()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, readReturnsFailureFails)
{
    mpt::TempDir temp_dir;

    std::string given_data{"some text"};
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto read_msg = make_msg(SFTP_READ);
    read_msg->offset = 0;
    read_msg->len = given_data.size();

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*file_ops, read(fd, _, _)).WillOnce(Return(-1));

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status,
            make_reply_status(read_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("read failed for"), HasSubstr(path.string()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, readReturnsZeroEndOfFile)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto read_msg = make_msg(SFTP_READ);
    read_msg->offset = 0;
    read_msg->len = 10;

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*file_ops, read(fd, _, _)).WillOnce(Return(0));

    REPLACE(sftp_handle, [&named_fd](auto...) { return (void*)&named_fd; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int eof_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(read_msg.get(), SSH_FX_EOF, eof_num_calls));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_EQ(eof_num_calls, 1);
}

TEST_F(SftpServer, handleExtendedLink)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("hardlink@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo info(link_name);
    EXPECT_TRUE(QFile::exists(link_name));
    EXPECT_TRUE(content_match(link_name, "this is a test file"));
}

TEST_F(SftpServer, extendedLinkInInvalidDirFails)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("hardlink@openssh.com");
    msg->submessage = submessage.data();
    auto invalid_path = name_as_char_array("bar");
    msg->filename = invalid_path.data();

    auto invalid_link = name_as_char_array("/foo/baz");
    REPLACE(sftp_client_message_get_data, [&invalid_link](auto...) { return invalid_link.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, extendedLinkFailureFails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("hardlink@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, link(_, _)).WillOnce(Return(false));

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    StrEq("sftp server"),
                    AllOf(HasSubstr("failed creating link from"),
                          HasSubstr(file_name.toStdString()),
                          HasSubstr(link_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, extendedLinkFailureFailsWhenSourceFileIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("hardlink@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);

    QFileInfo info(link_name);
    EXPECT_FALSE(QFile::exists(link_name));
}

TEST_F(SftpServer, handleExtendedRename)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("posix-rename@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_TRUE(QFile::exists(new_name));
    EXPECT_FALSE(QFile::exists(old_name));
}

TEST_F(SftpServer, extendedRenameFailsWhenMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("posix-rename@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp.run();

    ASSERT_THAT(perm_denied_num_calls, Eq(1));
    EXPECT_FALSE(QFile::exists(new_name));
    EXPECT_TRUE(QFile::exists(old_name));
}

TEST_F(SftpServer, extendedRenameInInvalidDirFails)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("posix-rename@openssh.com");
    msg->submessage = submessage.data();
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, invalidExtendedFails)
{
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("invalid submessage");
    msg->submessage = submessage.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OP_UNSUPPORTED, num_calls);

    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver();
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_P(Stat, handles)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto msg_type = GetParam();

    ASSERT_TRUE(MP_PLATFORM.symlink(file_name.toStdString().c_str(),
                                    link_name.toStdString().c_str(),
                                    QFileInfo(file_name).isDir()));
    ASSERT_TRUE(QFile::exists(link_name));
    ASSERT_TRUE(QFile::exists(file_name));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(msg_type);

    auto name = name_as_char_array(link_name.toStdString());
    msg->filename = name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    QFile file(file_name);
    uint64_t expected_size = msg_type == SFTP_LSTAT ? file_name.size() : file.size();
    auto reply_attr = [&num_calls, &msg, expected_size](sftp_client_message reply_msg,
                                                        sftp_attributes attr) {
        EXPECT_THAT(reply_msg, Eq(msg.get()));
        EXPECT_THAT(attr->size, Eq(expected_size));
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_attr, reply_attr);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_P(WhenInInvalidDir, fails)
{
    auto msg_type = GetParam();
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(msg_type);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status =
        make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

namespace
{
INSTANTIATE_TEST_SUITE_P(SftpServer,
                         Stat,
                         ::testing::Values(SFTP_LSTAT, SFTP_STAT),
                         string_for_message);
INSTANTIATE_TEST_SUITE_P(SftpServer,
                         WhenInInvalidDir,
                         ::testing::Values(SFTP_MKDIR,
                                           SFTP_RMDIR,
                                           SFTP_OPEN,
                                           SFTP_OPENDIR,
                                           SFTP_READLINK,
                                           SFTP_REALPATH,
                                           SFTP_REMOVE,
                                           SFTP_RENAME,
                                           SFTP_SETSTAT,
                                           SFTP_STAT),
                         string_for_message);

TEST_P(WhenInvalidMessageReceived, repliesFailure)
{
    mpt::TempDir temp_dir;

    auto params = GetParam();

    auto file_name = name_as_char_array(temp_dir.path().toStdString() +
                                        QDir::separator().toLatin1() + "this.does.not.exist");
    EXPECT_FALSE(QFile::exists(file_name.data()));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(params.message_type);
    msg->filename = file_name.data();

    auto data = name_as_char_array("");
    REPLACE(sftp_client_message_get_data, [&data](auto...) { return data.data(); });

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), params.reply_status_type, num_calls);

    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

INSTANTIATE_TEST_SUITE_P(SftpServer,
                         WhenInvalidMessageReceived,
                         ::testing::Values(MessageAndReply{SFTP_BAD_MESSAGE, SSH_FX_OP_UNSUPPORTED},
                                           MessageAndReply{SFTP_CLOSE, SSH_FX_BAD_MESSAGE},
                                           MessageAndReply{SFTP_READ, SSH_FX_BAD_MESSAGE},
                                           MessageAndReply{SFTP_FSETSTAT, SSH_FX_BAD_MESSAGE},
                                           MessageAndReply{SFTP_FSTAT, SSH_FX_BAD_MESSAGE},
                                           MessageAndReply{SFTP_READDIR, SSH_FX_BAD_MESSAGE},
                                           MessageAndReply{SFTP_WRITE, SSH_FX_BAD_MESSAGE},
                                           MessageAndReply{SFTP_OPENDIR, SSH_FX_NO_SUCH_FILE},
                                           MessageAndReply{SFTP_STAT, SSH_FX_NO_SUCH_FILE},
                                           MessageAndReply{SFTP_LSTAT, SSH_FX_NO_SUCH_FILE},
                                           MessageAndReply{SFTP_READLINK, SSH_FX_NO_SUCH_FILE},
                                           MessageAndReply{SFTP_SYMLINK, SSH_FX_PERMISSION_DENIED},
                                           MessageAndReply{SFTP_RENAME, SSH_FX_NO_SUCH_FILE},
                                           MessageAndReply{SFTP_SETSTAT, SSH_FX_NO_SUCH_FILE},
                                           MessageAndReply{SFTP_EXTENDED, SSH_FX_FAILURE}),
                         string_for_param);

TEST_P(PathValidation, validatesAccordingToRequest)
{
    mpt::TempDir temp_dir;
    auto params = GetParam();
    auto symlink_target = params.expected_path;
    auto full_input_path =
        temp_dir.path().toStdString() + QDir::separator().toLatin1() + params.input_path;

    if (!symlink_target.empty())
    {
        // Ensure parent directories exist
        QDir().mkpath(QFileInfo(QString::fromStdString(full_input_path)).path());

        MP_PLATFORM.symlink(symlink_target.c_str(), full_input_path.c_str(), false);
    }

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(params.message_type);
    auto name = name_as_char_array(params.input_path);
    msg->filename = name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls_status{0};
    auto reply_status = make_reply_status(msg.get(), params.expected_status, num_calls_status);
    REPLACE(sftp_reply_status, reply_status);

    int num_calls_attr{0};
    auto reply_attr =
        [&num_calls_attr, &msg, expected_status = params.expected_status](sftp_client_message m,
                                                                          sftp_attributes attr) {
            EXPECT_THAT(m, Eq(msg.get()));
            EXPECT_THAT(expected_status,
                        Eq(SSH_FX_OK)); // We only expect this if SSH_FX_OK was the goal
            ++num_calls_attr;
            return SSH_OK;
        };
    REPLACE(sftp_reply_attr, reply_attr);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls_status, Eq(1));
    EXPECT_THAT(num_calls_attr, Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    SftpServer,
    PathValidation,
    // If SSH_FX_NO_SUCH_FILE, the path passed validation
    ::testing::Values(
        // Fails validation because it tries to escape the mount
        PathTestData{SFTP_STAT, "../../../etc/passwd", "", SSH_FX_PERMISSION_DENIED},
        PathTestData{SFTP_LSTAT, "../../../etc/passwd", "", SSH_FX_PERMISSION_DENIED},
        // Fails because get_absolute_path returns /etc/passwd, which is outside source_path
        PathTestData{SFTP_STAT, "/etc/passwd", "", SSH_FX_PERMISSION_DENIED},
        // Passes validation. Since we didn't create it, returns NO_SUCH_FILE instead of
        // PERMISSION_DENIED.
        PathTestData{SFTP_STAT, "valid_relative_file.txt", "", SSH_FX_NO_SUCH_FILE},
        PathTestData{SFTP_LSTAT, "valid_relative_file.txt", "", SSH_FX_NO_SUCH_FILE},
        // We create a symlink named "malicious_link" that points to "/etc/passwd".
        // STAT follows the link. `weakly_canonical` resolves it to `/etc/passwd`. Validation fails.
        PathTestData{SFTP_STAT, "malicious_link", "/etc/passwd", SSH_FX_PERMISSION_DENIED},
        // LSTAT does NOT follow the link. `lexically_normal` resolves it to
        // `/mount/malicious_link`. Validation passes, and because the link physically exists, it
        // returns SSH_FX_OK (via sftp_reply_attr).
        PathTestData{SFTP_LSTAT, "malicious_link", "/etc/passwd", SSH_FX_OK}),
    string_for_pathdata);

TEST_P(AbsolutePath, normalizesToAbsolutePath)
{
    mpt::TempDir temp_dir;
    auto params = GetParam();

    auto host_path = fs::path{temp_dir.path().toStdString()};
    auto expected_path = host_path / params.expected_path;
    if (!expected_path.has_filename())
        expected_path = expected_path.parent_path();
    auto full_host_path = host_path / params.input_path;

    if (params.expected_status == SSH_FX_OK && !params.input_path.empty() &&
        params.input_path != ".")
    {
        // Ensure parent directories exist
        QDir().mkpath(QFileInfo(QString::fromStdString(full_host_path.string())).path());

        mpt::make_file_with_content(QString::fromStdString(full_host_path.string()));
    }

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(params.message_type);
    auto name = name_as_char_array(params.input_path);
    msg->filename = name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_status_calls{0};
    auto reply_status = make_reply_status(msg.get(), params.expected_status, num_status_calls);
    REPLACE(sftp_reply_status, reply_status);

    int num_name_calls{0};
    auto reply_name = [&num_name_calls,
                       &msg,
                       &expected_path,
                       expected_status = params.expected_status](sftp_client_message cmsg,
                                                                 const char* returned_name,
                                                                 sftp_attributes) {
        EXPECT_THAT(cmsg, Eq(msg.get()));
        EXPECT_THAT(std::string(returned_name), Eq(expected_path.generic_string()));
        EXPECT_THAT(expected_status,
                    Eq(SSH_FX_OK)); // We only expect this if SSH_FX_OK was the goal
        ++num_name_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_name, reply_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_name_calls, Eq(1));
    EXPECT_THAT(num_status_calls, Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    SftpServer,
    AbsolutePath,
    ::testing::Values(PathTestData{SFTP_REALPATH, ".", "", SSH_FX_OK},
                      PathTestData{SFTP_REALPATH, "./dir/../file.txt", "file.txt", SSH_FX_OK},
                      PathTestData{SFTP_REALPATH, "file.txt", "file.txt", SSH_FX_OK}),
    string_for_pathdata);

TEST_P(HostToGuestTranslation, translatesCorrectly)
{
    mpt::TempDir temp_dir;
    mpt::TempDir temp_dir2;
    auto params = GetParam();

    auto full_host_path =
        temp_dir.path().toStdString() + QDir::separator().toLatin1() + params.input_path;

    if (params.expected_status == SSH_FX_OK && !params.input_path.empty() &&
        params.input_path != ".")
    {
        // Ensure parent directories exist
        QDir().mkpath(QFileInfo(QString::fromStdString(full_host_path)).path());

        mpt::make_file_with_content(QString::fromStdString(full_host_path));
    }
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(params.message_type);
    auto name = name_as_char_array(params.input_path);
    msg->filename = name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_name_calls{0};
    int num_status_calls{0};

    auto expected_path =
        temp_dir2.path().toStdString() +
        (params.expected_path.empty() ? "" : QDir::separator().toLatin1() + params.expected_path);
    // We expect sftp_reply_name to be called with the translated guest path (temp2)
    // if within allowed mount path
    auto reply_name = [&num_name_calls,
                       &msg,
                       &expected_path,
                       expected_status = params.expected_status](sftp_client_message cmsg,
                                                                 const char* returned_name,
                                                                 sftp_attributes) {
        EXPECT_THAT(cmsg, Eq(msg.get()));
        EXPECT_THAT(std::string(returned_name), Eq(expected_path));
        EXPECT_THAT(expected_status,
                    Eq(SSH_FX_OK)); // We only expect this if SSH_FX_OK was the goal
        ++num_name_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_name, reply_name);

    // Otherwise, we expect an error
    auto reply_status = make_reply_status(msg.get(), params.expected_status, num_status_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(),
                                {{default_uid, mp::default_id}},
                                {{default_gid, mp::default_id}},
                                temp_dir2.path().toStdString());
    sftp.run();

    if (params.expected_status == SSH_FX_OK)
    {
        EXPECT_THAT(num_name_calls, Eq(1));
        EXPECT_THAT(num_status_calls, Eq(0));
    }
    else
    {
        EXPECT_THAT(num_status_calls, Eq(1));
        EXPECT_THAT(num_name_calls, Eq(0));
    }
}

INSTANTIATE_TEST_SUITE_P(
    SftpServer,
    HostToGuestTranslation,
    ::testing::Values(
        PathTestData{SFTP_REALPATH, "sub/dir/file.txt", "sub/dir/file.txt", SSH_FX_OK},
        PathTestData{SFTP_REALPATH, ".", "", SSH_FX_OK},
        PathTestData{SFTP_REALPATH, "../../../etc/passwd", "", SSH_FX_PERMISSION_DENIED},
        PathTestData{SFTP_REALPATH, "/etc/passwd", "", SSH_FX_PERMISSION_DENIED},
        PathTestData{SFTP_REALPATH, "valid_file.txt", "valid_file.txt", SSH_FX_OK}),
    string_for_pathdata);

TEST_F(SftpServer, AllowsPathWithinMount)
{
    mpt::TempDir temp_dir;

    std::string sibling_path = temp_dir.path().toStdString() + "/non_existent.txt";
    auto file_name = name_as_char_array(sibling_path);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SSH_FXP_OPENDIR);
    msg->filename = file_name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    // Path is validated, but file does not exist.
    // TODO: SSH_FX_NO_SUCH_DIR should be returned if the path is a dir, but it is not
    // the case.
    auto reply_status = make_reply_status(msg.get(), SSH_FX_NO_SUCH_FILE, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, symLinkInPathResolved)
{
    mpt::TempDir temp_dir;
    auto link = fs::path(temp_dir.path().toStdString()) / "linked";
    auto true_path = fs::path(temp_dir.path().toStdString()) / "real";
    auto expected_filename = (true_path / "file.txt").string();

    QDir().mkpath(QFileInfo(QString::fromStdString(true_path.string())).path());
    MP_PLATFORM.symlink((true_path / "").string().c_str(), link.string().c_str(), false);
    mpt::make_file_with_content(QString::fromStdString((true_path / "file.txt").string()));

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_REALPATH);
    auto filename = name_as_char_array((link / "file.txt").string());
    msg->filename = filename.data();

    int num_name_calls{};
    auto reply_name = [&num_name_calls, &msg, &expected_filename](sftp_client_message cmsg,
                                                                  const char* returned_name,
                                                                  sftp_attributes) {
        EXPECT_THAT(cmsg, Eq(msg.get()));
        EXPECT_THAT(std::string(returned_name), Eq(expected_filename));
        ++num_name_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_name, reply_name);
    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_name_calls, Eq(1));
    EXPECT_THAT(num_calls, Eq(0));
}

TEST_F(SftpServer, EmptyPathPermissionDenied)
{
    mpt::TempDir temp_dir;

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SSH_FXP_OPENDIR);
    msg->filename = nullptr;

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, BlocksSiblingDirectoryBypass)
{
    mpt::TempDir temp_dir;

    std::string sibling_path = temp_dir.path().toStdString() + "_malicious";
    auto file_name = name_as_char_array(sibling_path);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SSH_FXP_OPENDIR);
    msg->filename = file_name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, BlocksDirectoryTraversalEscape)
{
    mpt::TempDir temp_dir;

    std::string traversal_path = temp_dir.path().toStdString() + "/../../../../etc/passwd";
    auto file_name = name_as_char_array(traversal_path);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SSH_FXP_OPENDIR);
    msg->filename = file_name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, CanonicalErrorPermissionDenied)
{
    mpt::TempDir temp_dir;

    std::string traversal_path = temp_dir.path().toStdString();
    auto file_name = name_as_char_array(traversal_path);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SSH_FXP_OPENDIR);
    msg->filename = file_name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, weakly_canonical)
        .WillOnce([](const fs::path& path) { return fs::weakly_canonical(path); })
        .WillRepeatedly([](const fs::path& path) {
            throw fs::filesystem_error(std::string{}, std::error_code{});
            return fs::path();
        });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, RelativeErrorPermissionDenied)
{
    mpt::TempDir temp_dir;

    std::string traversal_path = temp_dir.path().toStdString();
    auto file_name = name_as_char_array(traversal_path);

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SSH_FXP_OPENDIR);
    msg->filename = file_name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, relative)
        .WillRepeatedly([](const fs::path& path, const fs::path& path2, std::error_code& ec) {
            throw fs::filesystem_error(std::string{}, std::error_code{});
            return fs::relative(path, path2, ec);
        });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, num_calls);
    REPLACE(sftp_reply_status, reply_status);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(mkdirChownHonorsMapsInTheHost))
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    int host_uid = QFileInfo(temp_dir.path()).ownerId();
    int host_gid = QFileInfo(temp_dir.path()).groupId();
    int sftp_uid = 1008;
    int sftp_gid = 1009;

    mp::id_mappings uid_mappings{{host_uid, sftp_uid}};
    mp::id_mappings gid_mappings{{host_gid, sftp_gid}};

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    msg->attr = &attr;
    msg->attr->uid = sftp_uid;
    msg->attr->gid = sftp_gid;

    REPLACE(sftp_get_client_message, make_msg_handler());

    EXPECT_CALL(*mock_platform, chown(_, host_uid, host_gid)).Times(1);
    EXPECT_CALL(*mock_platform, chown(_, sftp_uid, sftp_gid)).Times(0);
    EXPECT_CALL(*mock_platform, set_permissions(_, _, _)).WillRepeatedly(Return(true));

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), uid_mappings, gid_mappings);
    sftp.run();
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(mkdirChownWorksWhenIdsAreNotMapped))
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    msg->attr = &attr;
    msg->attr->uid = 1003;
    msg->attr->gid = 1004;

    REPLACE(sftp_get_client_message, make_msg_handler());

    QFileInfo parent_dir(temp_dir.path());

    EXPECT_CALL(*mock_platform, chown(_, parent_dir.ownerId(), parent_dir.groupId())).Times(1);
    EXPECT_CALL(*mock_platform, set_permissions(_, _, _)).WillRepeatedly(Return(true));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp.run();
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(openChownHonorsMapsInTheHost))
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    int host_uid = QFileInfo(temp_dir.path()).ownerId();
    int host_gid = QFileInfo(temp_dir.path()).groupId();
    int sftp_uid = 1008;
    int sftp_gid = 1009;

    mp::id_mappings uid_mappings{{host_uid, sftp_uid}};
    mp::id_mappings gid_mappings{{host_gid, sftp_gid}};
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_WRITE | SSH_FXF_CREAT;
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();
    msg->attr->uid = sftp_uid;
    msg->attr->gid = sftp_gid;

    REPLACE(sftp_get_client_message, make_msg_handler());

    EXPECT_CALL(*mock_platform, chown(_, host_uid, host_gid)).WillOnce(Return(-1));
    EXPECT_CALL(*mock_platform, chown(_, sftp_uid, sftp_gid)).Times(0);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), uid_mappings, gid_mappings);
    sftp.run();
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(setstatChownHonorsMapsInTheHost))
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    int host_uid = QFileInfo(temp_dir.path()).ownerId();
    int host_gid = QFileInfo(temp_dir.path()).groupId();
    int sftp_uid = 1024;
    int sftp_gid = 1025;

    mp::id_mappings uid_mappings{{host_uid, sftp_uid}};
    mp::id_mappings gid_mappings{{host_gid, sftp_gid}};
    auto init_msg = make_msg(SSH_FXP_INIT);
    auto msg = make_msg(SFTP_SETSTAT);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    const int expected_size = 7777;
    attr.size = expected_size;
    attr.flags = SSH_FILEXFER_ATTR_UIDGID;
    attr.permissions = 0777;

    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;
    msg->attr->uid = sftp_uid;
    msg->attr->gid = sftp_gid;

    REPLACE(sftp_get_client_message, make_msg_handler());

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, host_uid, host_gid)).Times(1);
    EXPECT_CALL(*mock_platform, chown(_, sftp_uid, sftp_gid)).Times(0);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), uid_mappings, gid_mappings);
    sftp.run();
}
} // namespace
