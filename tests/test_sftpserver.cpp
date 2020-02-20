/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include "file_operations.h"
#include "path.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sftp_server.h>

#include <QDateTime>

#include <multipass/format.h>
#include <gmock/gmock.h>

#include <queue>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

using StringUPtr = std::unique_ptr<ssh_string_struct, void (*)(ssh_string)>;

namespace
{
constexpr uint8_t SFTP_BAD_MESSAGE{255u};
struct SftpServer : public mp::test::SftpServerTest
{
    mp::SftpServer make_sftpserver()
    {
        return make_sftpserver("");
    }

    mp::SftpServer make_sftpserver(const std::string& path)
    {
        mp::SSHSession session{"a", 42};
        return {std::move(session), path, path, default_map, default_map, default_id, default_id, "sshfs"};
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

    mpt::ExitStatusMock exit_status_mock;
    std::queue<sftp_client_message> messages;
    std::unordered_map<int, int> default_map;
    int default_id{1000};
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
            exit_status_mock.return_exit_code(SSH_ERROR);
        }
        return SSH_OK;
    };

    REPLACE(ssh_channel_request_exec, request_exec);

    EXPECT_THROW(make_sftpserver(), std::runtime_error);
    EXPECT_TRUE(invoked);
}

TEST_F(SftpServer, sshfs_restarts_on_error)
{
    bool invoked{false};
    int num_calls{0};

    auto request_exec = [this, &invoked, &num_calls](ssh_channel, const char* raw_cmd) {
        std::string cmd{raw_cmd};
        if (cmd.find("sudo sshfs") != std::string::npos)
        {
            invoked = true;
            exit_status_mock.return_exit_code(SSH_OK);
            ++num_calls;
        }

        return SSH_OK;
    };

    REPLACE(ssh_channel_request_exec, request_exec);

    auto sftp = make_sftpserver();

    auto get_client_msg = [this, &num_calls](auto...) {
        if (num_calls == 1)
            exit_status_mock.return_exit_code(SSH_ERROR);

        return nullptr;
    };
    REPLACE(sftp_get_client_message, get_client_msg);

    sftp.run();

    EXPECT_THAT(num_calls, Eq(2));
    EXPECT_TRUE(invoked);
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

TEST_F(SftpServer, realpath_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_REALPATH);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, handles_opendir)
{
    auto dir_name = name_as_char_array(mpt::test_data_path().toStdString());

    auto sftp = make_sftpserver(mpt::test_data_path().toStdString());
    auto msg = make_msg(SFTP_OPENDIR);
    msg->filename = dir_name.data();

    bool reply_handle_invoked{false};
    auto reply_handle = [&reply_handle_invoked](auto...) {
        reply_handle_invoked = true;
        return SSH_OK;
    };
    REPLACE(sftp_reply_handle, reply_handle);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    EXPECT_TRUE(reply_handle_invoked);
}

TEST_F(SftpServer, opendir_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_OPENDIR);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, handles_mkdir)
{
    mpt::TempDir temp_dir;
    auto new_dir = fmt::format("{}/mkdir-test", temp_dir.path().toStdString());
    auto new_dir_name = name_as_char_array(new_dir);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_MKDIR);
    msg->filename = new_dir_name.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;

    int num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_OK, num_calls);
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_get_client_message, make_msg_handler());

    sftp.run();

    QDir expected_dir(new_dir_name.data());
    EXPECT_TRUE(expected_dir.exists());
    EXPECT_THAT(num_calls, Eq(1));
}

TEST_F(SftpServer, mkdir_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_MKDIR);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
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

TEST_F(SftpServer, rmdir_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RMDIR);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, handles_readlink)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto link_name = temp_dir.path() + "/test-link";
    mpt::make_file_with_content(file_name);

    ASSERT_TRUE(mp::platform::symlink(file_name.toStdString().c_str(), link_name.toStdString().c_str(),
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

TEST_F(SftpServer, readlink_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_READLINK);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
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

TEST_F(SftpServer, rename_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_RENAME);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
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

TEST_F(SftpServer, remove_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_REMOVE);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, open_in_write_mode_creates_file)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    ASSERT_FALSE(QFile::exists(file_name));

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_OPEN);
    msg->flags |= SSH_FXF_WRITE;
    sftp_attributes_struct attr{};
    attr.permissions = 0777;
    msg->attr = &attr;
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

    ASSERT_TRUE(reply_handle_invoked);
    EXPECT_TRUE(QFile::exists(file_name));
}

TEST_F(SftpServer, open_in_truncate_mode_truncates_file)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto size = mpt::make_file_with_content(file_name);

    ASSERT_TRUE(QFile::exists(file_name));
    ASSERT_THAT(size, Gt(0));

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
    EXPECT_THAT(file.size(), Eq(0));
}

TEST_F(SftpServer, open_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_OPEN);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, handles_readdir)
{
    mpt::TempDir temp_dir;
    QDir dir_entry(temp_dir.path());

    auto test_dir = temp_dir.path() + "/test-dir-entry";
    ASSERT_TRUE(dir_entry.mkdir(test_dir));

    auto test_file = temp_dir.path() + "/test-file";
    mpt::make_file_with_content(test_file);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto open_dir_msg = make_msg(SFTP_OPENDIR);
    auto dir_name = name_as_char_array(temp_dir.path().toStdString());
    open_dir_msg->filename = dir_name.data();

    auto readdir_msg = make_msg(SFTP_READDIR);
    auto readdir_msg_final = make_msg(SFTP_READDIR);

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return nullptr;
    };

    int eof_num_calls{0};
    auto reply_status = make_reply_status(readdir_msg_final.get(), SSH_FX_EOF, eof_num_calls);

    std::vector<std::string> entries;
    auto reply_names_add = [&entries](sftp_client_message msg, const char* file, const char* longname,
                                      sftp_attributes attr) {
        entries.push_back(file);
        return SSH_OK;
    };

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_reply_names_add, reply_names_add);
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

    sftp.run();

    EXPECT_THAT(eof_num_calls, Eq(1));

    std::vector<std::string> expected_entries = {".", "..", "test-dir-entry", "test-file"};
    EXPECT_THAT(entries, ContainerEq(expected_entries));
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
    auto open_dir_msg = make_msg(SFTP_OPENDIR);
    auto dir_name = name_as_char_array(temp_dir.path().toStdString());
    open_dir_msg->filename = dir_name.data();

    auto readdir_msg = make_msg(SFTP_READDIR);
    auto readdir_msg_final = make_msg(SFTP_READDIR);

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return nullptr;
    };

    int eof_num_calls{0};
    auto reply_status = make_reply_status(readdir_msg_final.get(), SSH_FX_EOF, eof_num_calls);

    std::unique_ptr<sftp_attributes_struct, void (*)(sftp_attributes_struct*)> test_file_attrs(
        new sftp_attributes_struct, [](sftp_attributes_struct* s) { delete s; });

    auto get_test_file_attributes = [&test_file_attrs, &test_file_name](sftp_client_message msg, const char* file,
                                                                        const char* longname, sftp_attributes attr) {
        if (strcmp(file, test_file_name) == 0)
        {
            memcpy(test_file_attrs.get(), attr, sizeof(sftp_attributes_struct));
        }
        return SSH_OK;
    };

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);
    REPLACE(sftp_reply_names_add, get_test_file_attributes);
    REPLACE(sftp_reply_names, [](auto...) { return SSH_OK; });

    sftp.run();

    EXPECT_THAT(eof_num_calls, Eq(1));

    QFileInfo test_file_info(test_file);
    EXPECT_EQ(test_file_attrs->size, (uint64_t)test_file_info.size());
    EXPECT_EQ(test_file_attrs->gid, test_file_info.groupId());
    EXPECT_EQ(test_file_attrs->uid, test_file_info.ownerId());
    EXPECT_EQ(test_file_attrs->atime,
              (uint32_t)test_file_info.lastModified().toSecsSinceEpoch()); // atime64 is zero, expected?

    EXPECT_TRUE(compare_permission(test_file_attrs->permissions, test_file_info, Permission::Owner));
    EXPECT_TRUE(compare_permission(test_file_attrs->permissions, test_file_info, Permission::Group));
    EXPECT_TRUE(compare_permission(test_file_attrs->permissions, test_file_info, Permission::Other));
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
        return nullptr;
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
    auto file_name = temp_dir.path() + "/test-file";
    uint64_t expected_size = mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto open_msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    open_msg->filename = name.data();
    open_msg->flags |= SSH_FXF_READ;

    auto fstat_msg = make_msg(SFTP_FSTAT);

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return nullptr;
    };

    int num_calls{0};
    auto reply_attr = [&num_calls, &fstat_msg, expected_size](sftp_client_message reply_msg, sftp_attributes attr) {
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
    open_msg->flags |= SSH_FXF_WRITE | SSH_FXF_TRUNC;

    auto fsetstat_msg = make_msg(SFTP_FSETSTAT);
    fsetstat_msg->attr = &attr;

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return nullptr;
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

TEST_F(SftpServer, setstat_in_invalid_dir_fails)
{
    mpt::TempDir temp_dir;

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto msg = make_msg(SFTP_SETSTAT);
    auto invalid_path = name_as_char_array("/foo/bar");
    msg->filename = invalid_path.data();

    int perm_denied_num_calls{0};
    auto reply_status = make_reply_status(msg.get(), SSH_FX_PERMISSION_DENIED, perm_denied_num_calls);

    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    EXPECT_THAT(perm_denied_num_calls, Eq(1));
}

TEST_F(SftpServer, handles_writes)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto open_msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    sftp_attributes_struct attr{};
    attr.permissions = 0777;

    open_msg->filename = name.data();
    open_msg->attr = &attr;
    open_msg->flags |= SSH_FXF_WRITE | SSH_FXF_TRUNC;

    auto write_msg1 = make_msg(SFTP_WRITE);
    auto data1 = make_data("The answer is ");
    write_msg1->data = data1.get();
    write_msg1->offset = 0;

    auto write_msg2 = make_msg(SFTP_WRITE);
    auto data2 = make_data("always 42");
    write_msg2->data = data2.get();
    write_msg2->offset = ssh_string_len(data1.get());

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return nullptr;
    };

    int num_calls{0};
    auto reply_status = [&num_calls](sftp_client_message, uint32_t status, const char*) {
        EXPECT_TRUE(status == SSH_FX_OK);
        ++num_calls;
        return SSH_OK;
    };

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_status, reply_status);

    sftp.run();

    ASSERT_THAT(num_calls, Eq(2));
    EXPECT_TRUE(content_match(file_name, "The answer is always 42"));
}

TEST_F(SftpServer, handles_reads)
{
    mpt::TempDir temp_dir;
    auto file_name = temp_dir.path() + "/test-file";
    auto size = mpt::make_file_with_content(file_name);

    auto sftp = make_sftpserver(temp_dir.path().toStdString());
    auto open_msg = make_msg(SFTP_OPEN);
    auto name = name_as_char_array(file_name.toStdString());
    open_msg->filename = name.data();
    open_msg->flags |= SSH_FXF_READ;

    auto read_msg = make_msg(SFTP_READ);
    read_msg->offset = 10;
    const int expected_size = size - read_msg->offset;
    read_msg->len = expected_size;

    void* id{nullptr};
    auto handle_alloc = [&id](sftp_session, void* info) {
        id = info;
        return nullptr;
    };

    int num_calls{0};
    auto reply_data = [&num_calls, &read_msg](sftp_client_message msg, const void* data, int len) {
        EXPECT_THAT(len, Gt(0));
        EXPECT_THAT(msg, Eq(read_msg.get()));

        std::string data_read{reinterpret_cast<const char*>(data), static_cast<std::string::size_type>(len)};
        EXPECT_THAT(data_read, StrEq("test file"));
        ++num_calls;
        return SSH_OK;
    };

    REPLACE(sftp_reply_handle, [](auto...) { return SSH_OK; });
    REPLACE(sftp_handle_alloc, handle_alloc);
    REPLACE(sftp_handle, [&id](auto...) { return id; });
    REPLACE(sftp_get_client_message, make_msg_handler());
    REPLACE(sftp_reply_data, reply_data);

    sftp.run();

    ASSERT_THAT(num_calls, Eq(1));
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

    ASSERT_TRUE(mp::platform::symlink(file_name.toStdString().c_str(), link_name.toStdString().c_str(),
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

namespace
{
INSTANTIATE_TEST_SUITE_P(SftpServer, Stat, ::testing::Values(SFTP_LSTAT, SFTP_STAT), string_for_message);

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

} // namespace
