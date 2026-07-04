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

#include <multipass/sshfs_mount/sftp_protocol.h>

#include <libssh/sftp.h>

#include <type_traits>

namespace mp = multipass;

namespace
{
// TODO@C++23 Convert casts to std::to_underlying

// Verify that our enums have the same width that libssh uses for the corresponding values
static_assert(std::is_same_v<std::underlying_type_t<mp::SftpMessageType>,
                             decltype(sftp_client_message_struct::type)>);
static_assert(std::is_same_v<std::underlying_type_t<mp::SftpStatus>,
                             decltype(sftp_status_message_struct::status)>);

// Verify that the constants we mirror match libssh's (both stand for SFTP wire values)
static_assert(static_cast<uint8_t>(mp::SftpMessageType::init) == SSH_FXP_INIT);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::version) == SSH_FXP_VERSION);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::open) == SSH_FXP_OPEN);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::close) == SSH_FXP_CLOSE);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::read) == SSH_FXP_READ);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::write) == SSH_FXP_WRITE);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::lstat) == SSH_FXP_LSTAT);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::fstat) == SSH_FXP_FSTAT);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::setstat) == SSH_FXP_SETSTAT);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::fsetstat) == SSH_FXP_FSETSTAT);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::opendir) == SSH_FXP_OPENDIR);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::readdir) == SSH_FXP_READDIR);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::remove) == SSH_FXP_REMOVE);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::mkdir) == SSH_FXP_MKDIR);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::rmdir) == SSH_FXP_RMDIR);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::realpath) == SSH_FXP_REALPATH);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::stat) == SSH_FXP_STAT);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::rename) == SSH_FXP_RENAME);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::readlink) == SSH_FXP_READLINK);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::symlink) == SSH_FXP_SYMLINK);
static_assert(static_cast<uint8_t>(mp::SftpMessageType::extended) == SSH_FXP_EXTENDED);

static_assert(static_cast<uint32_t>(mp::SftpStatus::ok) == SSH_FX_OK);
static_assert(static_cast<uint32_t>(mp::SftpStatus::eof) == SSH_FX_EOF);
static_assert(static_cast<uint32_t>(mp::SftpStatus::no_such_file) == SSH_FX_NO_SUCH_FILE);
static_assert(static_cast<uint32_t>(mp::SftpStatus::permission_denied) == SSH_FX_PERMISSION_DENIED);
static_assert(static_cast<uint32_t>(mp::SftpStatus::failure) == SSH_FX_FAILURE);
static_assert(static_cast<uint32_t>(mp::SftpStatus::bad_message) == SSH_FX_BAD_MESSAGE);
static_assert(static_cast<uint32_t>(mp::SftpStatus::no_connection) == SSH_FX_NO_CONNECTION);
static_assert(static_cast<uint32_t>(mp::SftpStatus::connection_lost) == SSH_FX_CONNECTION_LOST);
static_assert(static_cast<uint32_t>(mp::SftpStatus::op_unsupported) == SSH_FX_OP_UNSUPPORTED);

static_assert(mp::sftp_open_flags::read == SSH_FXF_READ);
static_assert(mp::sftp_open_flags::write == SSH_FXF_WRITE);
static_assert(mp::sftp_open_flags::append == SSH_FXF_APPEND);
static_assert(mp::sftp_open_flags::creat == SSH_FXF_CREAT);
static_assert(mp::sftp_open_flags::trunc == SSH_FXF_TRUNC);
static_assert(mp::sftp_open_flags::excl == SSH_FXF_EXCL);

static_assert(mp::sftp_attr_flags::size == SSH_FILEXFER_ATTR_SIZE);
static_assert(mp::sftp_attr_flags::uidgid == SSH_FILEXFER_ATTR_UIDGID);
static_assert(mp::sftp_attr_flags::permissions == SSH_FILEXFER_ATTR_PERMISSIONS);
static_assert(mp::sftp_attr_flags::acmodtime == SSH_FILEXFER_ATTR_ACMODTIME);
static_assert(mp::sftp_attr_flags::extended == SSH_FILEXFER_ATTR_EXTENDED);

static_assert(mp::sftp_file_mode::regular == SSH_S_IFREG);
static_assert(mp::sftp_file_mode::directory == SSH_S_IFDIR);
static_assert(mp::sftp_file_mode::symlink == SSH_S_IFLNK);
} // namespace
