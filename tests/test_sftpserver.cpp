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
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>

#include <queue>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
namespace mcp = multipass::cli::platform;

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

    mp::SftpServer make_sftpserver(const std::string& path,
                                   const mp::id_mappings& uid_mappings = {{default_uid, mp::default_id}},
                                   const mp::id_mappings& gid_mappings = {{default_gid, mp::default_id}})
    {
        mp::SSHSession session{"a", 42, "ubuntu", key_provider};
        return {std::move(session), path, path, gid_mappings, uid_mappings, default_uid, default_gid, "sshfs"};
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

    auto make_reply_status(sftp_client_message expected_msg, uint32_t expected_status, int& num_calls)
    {
        auto reply_status = [expected_msg, expected_status, &num_calls](sftp_client_message msg, uint32_t status,
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
    MessageAndReply(uint8_t type, uint32_t reply_status) : message_type{type}, reply_status_type{reply_status}
    {
    }
    uint8_t message_type;
    uint32_t reply_status_type;
};

struct WhenInvalidMessageReceived : public SftpServer, public ::testing::WithParamInterface<MessageAndReply>
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

std::string string_for_param(const ::testing::TestParamInfo<MessageAndReply>& info)
{
    return fmt::format("message_{}_replies_{}", name_for_message(info.param.message_type),
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

    // Comparing file permissions, sftp uses octal format: (aaabbbccc), QFileInfo uses hex format (aaaa----bbbbcccc)
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

    return ((ssh_permissions & ssh_perm_mask) >> ssh_bitshift) == ((file.permissions() & qt_perm_mask) >> qt_bitshift);
}
} // namespace

TEST_F(SftpServer, throws_when_failed_to_init)
{
    REPLACE(sftp_server_init, [](auto...) { return SSH_ERROR; });
    EXPECT_THROW(make_sftpserver(), std::runtime_error);
}

TEST_F(SftpServer, throws_when_sshfs_errors_on_start)
{
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

TEST_F(SftpServer, throws_on_ssh_failure_read_exit)
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

TEST_F(SftpServer, sshfs_restarts_on_timeout)
{
    int num_calls{0};
    auto request_exec = [this, &num_calls](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("sudo sshfs") != std::string::npos)
        {
            if (++num_calls < 3)
            {
                exit_status_mock.set_ssh_rc(SSH_OK);
                exit_status_mock.set_no_exit();
            }
        }

        return SSH_OK;
    };

    REPLACE(ssh_channel_request_exec, request_exec);

    auto sftp = make_sftpserver();

    auto get_client_msg = [this, &num_calls](auto...) {
        exit_status_mock.set_ssh_rc(SSH_OK);
        exit_status_mock.set_exit_status(num_calls == 1 ? exit_status_mock.failure_status
                                                        : exit_status_mock.success_status);

        return nullptr;
    };
    REPLACE(sftp_get_client_message, get_client_msg);

    sftp.run();

    EXPECT_EQ(num_calls, 2);
}

TEST_F(SftpServer, stops_after_a_null_message)
{
    auto sftp = make_sftpserver();

    REPLACE(sftp_get_client_message, [](auto...) { return nullptr; });
    sftp.run();
}

TEST_F(SftpServer, frees_message)
{
    auto sftp = make_sftpserver();

    auto msg = make_msg(SFTP_BAD_MESSAGE);

    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    msg_free.expectCalled(1).withValues(msg.get());
}

TEST_F(SftpServer, handles_realpath)
{
    mpt::TempFile file;
    auto file_name = name_as_char_array(file.name().toStdString());

    auto sftp = make_sftpserver(file.name().toStdString());
    auto msg = make_msg(SFTP_REALPATH);
    msg->filename = file_name.data();

    bool invoked{false};
    auto reply_name = [&msg, &invoked, &file_name](sftp_client_message cmsg, const char* name, sftp_attributes attr) {
        EXPECT_THAT(cmsg, Eq(msg.get()));
        EXPECT_THAT(name, StrEq(file_name.data()));
        invoked = true;
        return SSH_OK;
    };
    REPLACE(sftp_reply_name, reply_name);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_TRUE(invoked);
}

TEST_F(SftpServer, realpathFailsWhenIdsAreNotMapped)
{
    mpt::TempFile file;
    auto file_name = name_as_char_array(file.name().toStdString());

    auto sftp = make_sftpserver(file.name().toStdString(), {}, {});
    auto msg = make_msg(SFTP_REALPATH);
    msg->filename = file_name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handles_opendir)
{
    auto dir_name = name_as_char_array(mpt::test_data_path().toStdString());

    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce(Return(std::make_unique<mpt::MockDirIterator>()));

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();
}

TEST_F(SftpServer, opendir_not_existing_fails)
{
    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    auto dir_name = name_as_char_array(mpt::test_data_path().toStdString());
    const auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce([&](const mp::fs::path&, std::error_code& err) {
        err = std::make_error_code(std::errc::no_such_file_or_directory);
        return std::make_unique<mpt::MockDirIterator>();
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int no_such_file_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_NO_SUCH_FILE, no_such_file_calls));

    sftp.run();

    EXPECT_EQ(no_such_file_calls, 1);
}

TEST_F(SftpServer, opendir_not_readable_fails)
{
    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    auto dir_name = name_as_char_array(mpt::test_data_path().toStdString());
    const auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return std::make_unique<mpt::MockDirIterator>();
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int perm_denied_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("Cannot read directory"), HasSubstr(mpt::test_data_path().toStdString())))));

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, opendir_no_handle_allocated_fails)
{
    auto dir_name = name_as_char_array(mpt::test_data_path().toStdString());

    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, dir_iterator).WillOnce([&](const mp::fs::path&, std::error_code& err) {
        err.clear();
        return std::make_unique<mpt::MockDirIterator>();
    });
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });

    REPLACE(sftp_handle_alloc, [](auto...) { return nullptr; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Cannot allocate handle for opendir()"))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, opendirFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto open_dir_msg = make_msg(SFTP_OPENDIR);
    auto dir_name = name_as_char_array(temp_dir.path().toStdString());
    open_dir_msg->filename = dir_name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(open_dir_msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handles_mkdir)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    const auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    msg->attr = &attr;

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, permissions(A<const mp::fs::path&>(), _, _)).WillOnce([](auto, auto, std::error_code& err) {
        err.clear();
    });
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });

    int num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_OK, num_calls));
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_TRUE(QDir{new_dir_name.data()}.exists());
    EXPECT_EQ(num_calls, 1);
}

TEST_F(SftpServer, mkdir_on_existing_dir_fails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    QDir dir(new_dir_name.data());
    ASSERT_TRUE(dir.mkdir(new_dir_name.data()));
    ASSERT_TRUE(dir.exists());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("mkdir failed for"), HasSubstr(new_dir)))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, mkdir_set_permissions_fails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, permissions(_, _, _))
        .WillOnce(SetArgReferee<2>(std::make_error_code(std::errc::operation_not_permitted)));
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    msg->attr = &attr;

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
            mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("set permissions failed for"), HasSubstr(new_dir)))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, mkdir_chown_failure_fails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, _, _)).WillOnce(Return(-1));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("failed to chown"), HasSubstr(new_dir)))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, mkdirFailsInDirThatsMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    const auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    msg->attr = &attr;

    int perm_denied_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls));
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_FALSE(QDir{new_dir_name.data()}.exists());
    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handles_rmdir)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    QDir dir(new_dir_name.data());
    ASSERT_TRUE(dir.mkdir(new_dir_name.data()));
    ASSERT_TRUE(dir.exists());

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_FALSE(dir.exists());
    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, rmdir_non_existing_fails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("rmdir failed for"), HasSubstr(new_dir)))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, rmdir_unable_to_remove_fails)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, remove(_, _)).WillOnce(Return(false));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("rmdir failed for"), HasSubstr(new_dir)))));

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

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_RMDIR);
    msg->filename = new_dir_name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_TRUE(dir.exists());
    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handles_readlink)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(MP_PLATFORM.symlink(file_name.toStdString().c_str(), link_name.toStdString().c_str(),
                                    QFileInfo(file_name).isDir()));
    ASSERT_TRUE(QFile::exists(link_name));
    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_READLINK);
    auto name = name_as_char_array(link_name.toStdString());
    msg->filename = name.data();

    int num_calls{0};
    auto names_add = [&num_calls, &msg, &file_name](sftp_client_message reply_msg, const char* file, const char*,
                                                    sftp_attributes) {
        EXPECT_THAT(reply_msg, Eq(msg.get()));
        EXPECT_THAT(file, StrEq(file_name.toStdString()));
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_names_add, names_add);
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

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

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_READLINK);
    auto name = name_as_char_array(link_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handles_symlink)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo info(link_name);
    EXPECT_TRUE(QFile::exists(link_name));
    EXPECT_TRUE(info.isSymLink());
    EXPECT_THAT(info.symLinkTarget(), Eq(file_name));
}

TEST_F(SftpServer, symlink_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_SYMLINK);
    auto target = name_as_char_array("bar");
    msg->filename = target.data();

    auto invalid_link = name_as_char_array("/foo/baz");
    REPLACE(sftp_client_message_get_data, [&invalid_link](auto...) { return invalid_link.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, broken_symlink_does_not_fail)
{
    mpt::TempDir temp_dir;
    auto missing_file_name = temp_dir.path() + "/test-file";
    auto broken_link_name = temp_dir.path() + "/test-link";

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo info(broken_link_name);
    EXPECT_TRUE(info.isSymLink());
    EXPECT_FALSE(QFile::exists(info.symLinkTarget()));
    EXPECT_FALSE(QFile::exists(missing_file_name));
}

TEST_F(SftpServer, symlink_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("failure creating symlink from"),
                                                                HasSubstr(file_name.toStdString()),
                                                                HasSubstr(link_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, symlinkFailsWhenMissingMappedIds)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
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
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    ASSERT_THAT(perm_denied_num_calls, Eq(1));

    QFileInfo info(link_name);
    EXPECT_FALSE(QFile::exists(link_name));
    EXPECT_FALSE(info.isSymLink());
}

TEST_F(SftpServer, handles_rename)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_TRUE(QFile::exists(new_name));
    EXPECT_FALSE(QFile::exists(old_name));
}

TEST_F(SftpServer, rename_cannot_remove_target_fails)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);
    mpt::make_file_with_content(new_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, remove(_)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>())).WillRepeatedly([](const QFileInfo& file) {
        return file.exists();
    });

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(
                        HasSubstr("cannot remove"), HasSubstr(new_name.toStdString()), HasSubstr("for renaming")))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, rename_failure_fails)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    const auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, rename(_, _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>())).WillRepeatedly([](const QFileInfo& file) {
        return file.exists();
    });

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
            mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("failed renaming"), HasSubstr(old_name.toStdString()),
                                                        HasSubstr(new_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, rename_invalid_target_fails)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto invalid_target = name_as_char_array("/foo/bar");
    mpt::make_file_with_content(old_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    REPLACE(sftp_client_message_get_data, [&invalid_target](auto...) { return invalid_target.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, renameFailsWhenSourceFileIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(HasSubstr(old_name.toStdString()))));

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
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillOnce([](const QFileInfo& file) { return file.groupId(); });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>())).WillRepeatedly([](const QFileInfo& file) {
        return file.exists();
    });

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RENAME);
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(HasSubstr(new_name.toStdString()))));

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_TRUE(QFile::exists(old_name));
    EXPECT_TRUE(QFile::exists(new_name));
}

TEST_F(SftpServer, handles_remove)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_REMOVE);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_FALSE(QFile::exists(file_name));
}

TEST_F(SftpServer, remove_non_existing_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_REMOVE);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("cannot remove"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, removeFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_REMOVE);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_TRUE(QFile::exists(file_name));
}

TEST_F(SftpServer, open_in_write_mode_creates_file)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto name = name_as_char_array(file_name.toStdString());
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

    sftp.run();

    ASSERT_TRUE(reply_handle_invoked);
    EXPECT_TRUE(QFile::exists(file_name));
}

TEST_F(SftpServer, open_in_truncate_mode_truncates_file)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    QFile file(file_name);
    ASSERT_TRUE(reply_handle_invoked);
    EXPECT_EQ(file.size(), 0);
}

TEST_F(SftpServer, open_unable_to_open_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto name = name_as_char_array(file_name.toStdString());
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
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("Cannot open"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, open_unable_to_get_status_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    auto name = name_as_char_array(file_name.toStdString());
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_READ | SSH_FXF_WRITE | SSH_FXF_APPEND | SSH_FXF_EXCL;
    msg->attr = &attr;
    msg->filename = name.data();

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*file_ops, symlink_status).WillOnce([](auto, std::error_code& err) {
        err = std::make_error_code(std::errc::permission_denied);
        return mp::fs::file_status{mp::fs::file_type::unknown};
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("Cannot get status"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, open_chown_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_platform, chown(_, _, _)).WillOnce(Return(-1));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("failed to chown"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, open_no_handle_allocated_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Cannot allocate handle for open()"))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, openFailsWhenIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, openNonExistingFileFailsWhenDirIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    QFile file(file_name);
    EXPECT_FALSE(file.exists());
}

TEST_F(SftpServer, handles_readdir)
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

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    auto readdir_msg = make_msg(SFTP_READDIR);
    auto readdir_msg_final = make_msg(SFTP_READDIR);

    std::vector<mp::fs::path> expected_entries = {".", "..", "test-dir-entry", "test-file", "test-link"};
    auto entries_read = 0ul;

    auto directory_entry = mpt::MockDirectoryEntry{};
    EXPECT_CALL(directory_entry, path).WillRepeatedly([&]() -> const mp::fs::path& {
        return expected_entries[entries_read - 1];
    });
    EXPECT_CALL(directory_entry, is_symlink()).WillRepeatedly([&]() {
        return expected_entries[entries_read - 1] == "test-link";
    });
    auto dir_iterator = mpt::MockDirIterator{};
    EXPECT_CALL(dir_iterator, hasNext).WillRepeatedly([&] { return entries_read != expected_entries.size(); });
    EXPECT_CALL(dir_iterator, next).WillRepeatedly(DoAll([&] { entries_read++; }, ReturnRef(directory_entry)));

    REPLACE(sftp_handle, [&dir_iterator](auto...) { return &dir_iterator; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int eof_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(readdir_msg_final.get(), SSH_FX_EOF, eof_num_calls));

    std::vector<mp::fs::path> given_entries;
    auto reply_names_add = [&given_entries](auto, const char* file, auto, auto) {
        given_entries.push_back(file);
        return SSH_OK;
    };
    REPLACE(sftp_reply_names_add, reply_names_add);
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

    sftp.run();

    EXPECT_EQ(eof_num_calls, 1);
    EXPECT_THAT(given_entries, ContainerEq(expected_entries));
}

TEST_F(SftpServer, handles_readdir_attributes_preserved)
{
    mpt::TempDir temp_dir;
    QDir dir_entry(temp_dir.path());

    const auto test_file_name = "test-file";
    auto test_file = temp_dir.path() + "/" + test_file_name;
    mpt::make_file_with_content(test_file, "some content for the file to give it non-zero size");

    QFileDevice::Permissions expected_permissions =
        QFileDevice::WriteOwner | QFileDevice::ExeGroup | QFileDevice::ReadOther;
    QFile::setPermissions(test_file, expected_permissions);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

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
    EXPECT_CALL(dir_iterator, hasNext).WillRepeatedly([&] { return entries_read != expected_entries.size(); });
    EXPECT_CALL(dir_iterator, next).WillRepeatedly(DoAll([&] { entries_read++; }, ReturnRef(directory_entry)));

    REPLACE(sftp_handle, [&dir_iterator](auto...) { return &dir_iterator; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    int eof_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(readdir_msg_final.get(), SSH_FX_EOF, eof_num_calls));

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

    sftp.run();

    EXPECT_EQ(eof_num_calls, 1);

    QFileInfo test_file_info(test_file);
    EXPECT_EQ(test_file_attrs.size, (uint64_t)test_file_info.size());
    EXPECT_EQ(test_file_attrs.gid, test_file_info.groupId());
    EXPECT_EQ(test_file_attrs.uid, test_file_info.ownerId());
    EXPECT_EQ(test_file_attrs.atime,
              (uint32_t)test_file_info.lastModified().toSecsSinceEpoch()); // atime64 is zero, expected?

    EXPECT_TRUE(compare_permission(test_file_attrs.permissions, test_file_info, Permission::Owner));
    EXPECT_TRUE(compare_permission(test_file_attrs.permissions, test_file_info, Permission::Group));
    EXPECT_TRUE(compare_permission(test_file_attrs.permissions, test_file_info, Permission::Other));
}

TEST_F(SftpServer, handles_close)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    EXPECT_THAT(ok_num_calls, Eq(1));
}

TEST_F(SftpServer, handles_fstat)
{
    mpt::TempDir temp_dir;
    const auto content = std::string{"whatever just some content bla bla"};
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name, content);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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
    auto reply_attr = [&num_calls, &fstat_msg, expected_size = content.size()](sftp_client_message reply_msg,
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

    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, handles_fsetstat)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    QFile file(file_name);
    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_TRUE(file.exists());
    EXPECT_THAT(file.size(), Eq(expected_size));
}

TEST_F(SftpServer, handles_setstat)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    QFile file(file_name);
    ASSERT_THAT(num_calls, Eq(1));
    EXPECT_THAT(file.size(), Eq(expected_size));
}

TEST_F(SftpServer, setstat_correctly_modifies_file_timestamp)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    QFileInfo orig_info{file_name};
    auto original_time = orig_info.lastModified().toSecsSinceEpoch();

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo modified_info{file_name};
    auto new_time = modified_info.lastModified().toSecsSinceEpoch();
    EXPECT_EQ(new_time, original_time + 1);
}

TEST_F(SftpServer, setstat_resize_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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
    EXPECT_CALL(*mock_file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*mock_file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });
    EXPECT_CALL(*mock_file_ops, exists(A<const QFileInfo&>())).WillRepeatedly([](const QFileInfo& file) {
        return file.exists();
    });

    int failure_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("cannot resize"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstat_set_permissions_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    attr.size = 7777;
    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = 0777;

    auto msg = make_msg(SFTP_SETSTAT);
    msg->filename = name.data();
    msg->attr = &attr;
    msg->flags = SSH_FXF_WRITE;

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, resize).WillOnce(Return(true));
    EXPECT_CALL(*file_ops, permissions(_, _, _))
        .WillOnce(SetArgReferee<2>(std::make_error_code(std::errc::permission_denied)));
    EXPECT_CALL(*file_ops, ownerId(_)).WillRepeatedly([](const QFileInfo& file) { return file.ownerId(); });
    EXPECT_CALL(*file_ops, groupId(_)).WillRepeatedly([](const QFileInfo& file) { return file.groupId(); });
    EXPECT_CALL(*file_ops, exists(A<const QFileInfo&>())).WillRepeatedly([](const QFileInfo& file) {
        return file.exists();
    });

    REPLACE(sftp_get_client_message, make_msg_handler());
    int failure_num_calls{0};
    REPLACE(sftp_reply_status, make_reply_status(msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("set permissions failed for"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstat_chown_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(),
                                {{default_uid, -1}, {1001, 1001}},
                                {{default_gid, -1}, {1001, 1001}});
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

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("cannot set ownership for"), HasSubstr(file_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, setstat_utime_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr("cannot set modification date for"), HasSubstr(file_name.toStdString())))));

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

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
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
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
    EXPECT_EQ(file.size(), file_size);
}

TEST_F(SftpServer, setstatChownFailsWhenNewIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);
}

TEST_F(SftpServer, handles_writes)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

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
    EXPECT_CALL(*file_ops, write(fd, _, _)).WillRepeatedly([&stream](int, const void* buf, size_t nbytes) {
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

    sftp.run();

    ASSERT_EQ(num_calls, 2);
    EXPECT_EQ(stream.str(), "The answer is always 42");
}

TEST_F(SftpServer, write_cannot_seek_fails)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

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
    REPLACE(sftp_reply_status, make_reply_status(write_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, write_failure_fails)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

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
    REPLACE(sftp_reply_status, make_reply_status(write_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, handles_reads)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    std::string given_data{"some text"};
    auto read_msg = make_msg(SFTP_READ);
    read_msg->offset = 0;
    read_msg->len = given_data.size();

    const auto path = mp::fs::path{temp_dir.path().toStdString()} / "test-file";
    const auto fd = 123;
    const auto named_fd = std::make_pair(path, fd);

    const auto [file_ops, mock_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*file_ops, lseek(fd, _, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*file_ops, read(fd, _, _)).WillRepeatedly([&given_data, r = 0](int, void* buf, size_t count) mutable {
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

        std::string data_read{reinterpret_cast<const char*>(data), static_cast<std::string::size_type>(len)};
        EXPECT_EQ(data_read, given_data);
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_data, reply_data);

    sftp.run();

    ASSERT_EQ(num_calls, 1);
}

TEST_F(SftpServer, read_cannot_seek_fails)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    std::string given_data{"some text"};
    const int seek_pos{10};
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
    REPLACE(sftp_reply_status, make_reply_status(read_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(mpl::Level::trace,
            mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
            mpt::MockLogger::make_cstring_matcher(
                AllOf(HasSubstr(fmt::format("cannot seek to position {} in", seek_pos)), HasSubstr(path.string())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, read_returns_failure_fails)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    std::string given_data{"some text"};
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
    REPLACE(sftp_reply_status, make_reply_status(read_msg.get(), SSH_FX_FAILURE, failure_num_calls));

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::trace),
            mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
            mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("read failed for"), HasSubstr(path.string())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, read_returns_zero_end_of_file)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

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

    sftp.run();

    EXPECT_EQ(eof_num_calls, 1);
}

TEST_F(SftpServer, handle_extended_link)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));

    QFileInfo info(link_name);
    EXPECT_TRUE(QFile::exists(link_name));
    EXPECT_TRUE(content_match(link_name, "this is a test file"));
}

TEST_F(SftpServer, extended_link_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("hardlink@openssh.com");
    msg->submessage = submessage.data();
    auto invalid_path = name_as_char_array("bar");
    msg->filename = invalid_path.data();

    auto invalid_link = name_as_char_array("/foo/baz");
    REPLACE(sftp_client_message_get_data, [&invalid_link](auto...) { return invalid_link.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, extended_link_failure_fails)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    logger_scope.mock_logger->screen_logs(mpl::Level::trace);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("sftp server")),
                    mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("failed creating link from"),
                                                                HasSubstr(file_name.toStdString()),
                                                                HasSubstr(link_name.toStdString())))));

    sftp.run();

    EXPECT_EQ(failure_num_calls, 1);
}

TEST_F(SftpServer, extendedLinkFailureFailsWhenSourceFileIdsAreNotMapped)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("hardlink@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(file_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(link_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_EQ(perm_denied_num_calls, 1);

    QFileInfo info(link_name);
    EXPECT_FALSE(QFile::exists(link_name));
}

TEST_F(SftpServer, handle_extended_rename)
{
    mpt::TempDir temp_dir;
    auto old_name = temp_dir.path() + "/test-file";
    auto new_name = temp_dir.path() + "/test-renamed";
    mpt::make_file_with_content(old_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
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

    auto sftp = make_sftpserver(temp_dir.path().toStdString(), {}, {});
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("posix-rename@openssh.com");
    msg->submessage = submessage.data();
    auto name = name_as_char_array(old_name.toStdString());
    msg->filename = name.data();

    auto target_name = name_as_char_array(new_name.toStdString());
    REPLACE(sftp_client_message_get_data, [&target_name](auto...) { return target_name.data(); });

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    ASSERT_THAT(perm_denied_num_calls, Eq(1));
    EXPECT_FALSE(QFile::exists(new_name));
    EXPECT_TRUE(QFile::exists(old_name));
}

TEST_F(SftpServer, extended_rename_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("posix-rename@openssh.com");
    msg->submessage = submessage.data();
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, invalid_extended_fails)
{
    auto sftp = make_sftpserver();

    auto msg = make_msg(SFTP_EXTENDED);
    auto submessage = name_as_char_array("invalid submessage");
    msg->submessage = submessage.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OP_UNSUPPORTED, num_calls);

    REPLACE(sftp_reply_status, reply_status);

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

    ASSERT_TRUE(MP_PLATFORM.symlink(file_name.toStdString().c_str(), link_name.toStdString().c_str(),
                                    QFileInfo(file_name).isDir()));
    ASSERT_TRUE(QFile::exists(link_name));
    ASSERT_TRUE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(msg_type);

    auto name = name_as_char_array(link_name.toStdString());
    msg->filename = name.data();

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    QFile file(file_name);
    uint64_t expected_size = msg_type == SFTP_LSTAT ? file_name.size() : file.size();
    auto reply_attr = [&num_calls, &msg, expected_size](sftp_client_message reply_msg, sftp_attributes attr) {
        EXPECT_THAT(reply_msg, Eq(msg.get()));
        EXPECT_THAT(attr->size, Eq(expected_size));
        ++num_calls;
        return SSH_OK;
    };
    REPLACE(sftp_reply_attr, reply_attr);

    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

TEST_P(WhenInInvalidDir, fails)
{
    auto msg_type = GetParam();
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(msg_type);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

namespace
{
INSTANTIATE_TEST_SUITE_P(SftpServer, Stat, ::testing::Values(SFTP_LSTAT, SFTP_STAT), string_for_message);
INSTANTIATE_TEST_SUITE_P(SftpServer, WhenInInvalidDir,
                         ::testing::Values(SFTP_MKDIR, SFTP_RMDIR, SFTP_OPEN, SFTP_OPENDIR, SFTP_READLINK,
                                           SFTP_REALPATH, SFTP_REMOVE, SFTP_RENAME, SFTP_SETSTAT, SFTP_STAT),
                         string_for_message);

TEST_P(WhenInvalidMessageReceived, replies_failure)
{
    mpt::TempDir temp_dir;
    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    auto params = GetParam();

    auto file_name = name_as_char_array(temp_dir.path().toStdString() + "this.does.not.exist");
    EXPECT_FALSE(QFile::exists(file_name.data()));

    auto msg = make_msg(params.message_type);
    msg->filename = file_name.data();

    auto data = name_as_char_array("");
    REPLACE(sftp_client_message_get_data, [&data](auto...) { return data.data(); });

    REPLACE(sftp_get_client_message, make_msg_handler());

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), params.reply_status_type, num_calls);

    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(num_calls, Eq(1));
}

INSTANTIATE_TEST_SUITE_P(
    SftpServer, WhenInvalidMessageReceived,
    ::testing::Values(
        MessageAndReply{SFTP_BAD_MESSAGE, SSH_FX_OP_UNSUPPORTED}, MessageAndReply{SFTP_CLOSE, SSH_FX_BAD_MESSAGE},
        MessageAndReply{SFTP_READ, SSH_FX_BAD_MESSAGE}, MessageAndReply{SFTP_FSETSTAT, SSH_FX_BAD_MESSAGE},
        MessageAndReply{SFTP_FSTAT, SSH_FX_BAD_MESSAGE}, MessageAndReply{SFTP_READDIR, SSH_FX_BAD_MESSAGE},
        MessageAndReply{SFTP_WRITE, SSH_FX_BAD_MESSAGE}, MessageAndReply{SFTP_OPENDIR, SSH_FX_NO_SUCH_FILE},
        MessageAndReply{SFTP_STAT, SSH_FX_NO_SUCH_FILE}, MessageAndReply{SFTP_LSTAT, SSH_FX_NO_SUCH_FILE},
        MessageAndReply{SFTP_READLINK, SSH_FX_NO_SUCH_FILE}, MessageAndReply{SFTP_SYMLINK, SSH_FX_PERMISSION_DENIED},
        MessageAndReply{SFTP_RENAME, SSH_FX_NO_SUCH_FILE}, MessageAndReply{SFTP_SETSTAT, SSH_FX_NO_SUCH_FILE},
        MessageAndReply{SFTP_EXTENDED, SSH_FX_FAILURE}),
    string_for_param);

TEST_F(SftpServer, DISABLE_ON_WINDOWS(mkdir_chown_honors_maps_in_the_host))
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
    auto sftp = make_sftpserver(temp_dir.path().toStdString(), uid_mappings, gid_mappings);

    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    msg->attr = &attr;
    msg->attr->uid = sftp_uid;
    msg->attr->gid = sftp_gid;

    REPLACE(sftp_get_client_message, make_msg_handler());

    EXPECT_CALL(*mock_platform, chown(_, host_uid, host_gid)).Times(1);
    EXPECT_CALL(*mock_platform, chown(_, sftp_uid, sftp_gid)).Times(0);

    sftp.run();
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(mkdir_chown_works_when_ids_are_not_mapped))
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    const auto [mock_platform, guard] = mpt::MockPlatform::inject();

    auto sftp = make_sftpserver(temp_dir.path().toStdString());

    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    msg->attr = &attr;
    msg->attr->uid = 1003;
    msg->attr->gid = 1004;

    REPLACE(sftp_get_client_message, make_msg_handler());

    QFileInfo parent_dir(temp_dir.path());

    EXPECT_CALL(*mock_platform, chown(_, parent_dir.ownerId(), parent_dir.groupId())).Times(1);

    sftp.run();
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(open_chown_honors_maps_in_the_host))
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
    auto sftp = make_sftpserver(temp_dir.path().toStdString(), uid_mappings, gid_mappings);
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

    sftp.run();
}

TEST_F(SftpServer, DISABLE_ON_WINDOWS(setstat_chown_honors_maps_in_the_host))
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
    auto sftp = make_sftpserver(temp_dir.path().toStdString(), uid_mappings, gid_mappings);
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

    sftp.run();
}
} // namespace
