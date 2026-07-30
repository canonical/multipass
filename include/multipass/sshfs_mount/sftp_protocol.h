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

/**
 * @file
 * @brief SFTP protocol vocabulary — constants and attributes, as per version 3 of the protocol.
 *
 * Note: although versions go up to 6, version 3 is the defacto standard that was adopted by
 * openssh (and libssh). It is specified in a draft.
 *
 * @see https://datatracker.ietf.org/doc/html/draft-ietf-secsh-filexfer-02
 * @see https://en.wikipedia.org/wiki/SSH_File_Transfer_Protocol
 * @see https://github.com/openssh/openssh-portable/blob/master/sftp.h#L31-L32
 * @see https://api.libssh.org/stable/libssh_tutor_sftp.html
 */

#pragma once

#include <cstdint>

namespace multipass
{

/**
 * Type of SFTP request (SSH_FXP_* in the protocol spec).
 */
enum class SftpMessageType : uint8_t
{
    init = 1,
    version = 2,
    open = 3,
    close = 4,
    read = 5,
    write = 6,
    lstat = 7,
    fstat = 8,
    setstat = 9,
    fsetstat = 10,
    opendir = 11,
    readdir = 12,
    remove = 13,
    mkdir = 14,
    rmdir = 15,
    realpath = 16,
    stat = 17,
    rename = 18,
    readlink = 19,
    symlink = 20,
    extended = 200,
};

/**
 * Status code for an SFTP status reply (SSH_FX_* in the protocol spec).
 */
enum class SftpStatus : uint32_t
{
    ok = 0,
    eof = 1,
    no_such_file = 2,
    permission_denied = 3,
    failure = 4,
    bad_message = 5,
    no_connection = 6,
    connection_lost = 7,
    op_unsupported = 8,
};

/**
 * Access flags of an open request (SSH_FXF_* in the protocol spec); see SftpMessage::flags.
 */
struct SftpOpenFlags
{
    enum : uint32_t
    {
        read = 0x01,
        write = 0x02,
        append = 0x04,
        creat = 0x08,
        trunc = 0x10,
        excl = 0x20,
    };
};

/**
 * Validity flags for SftpAttributes fields (SSH_FILEXFER_ATTR_* in the protocol spec).
 */
struct SftpAttrFlags
{
    enum : uint32_t
    {
        size = 0x00000001,
        uidgid = 0x00000002,
        permissions = 0x00000004,
        acmodtime = 0x00000008,
        extended = 0x80000000,
    };
};

/**
 * POSIX-style file-type bits, encoded in SftpAttributes::permissions (SSH_S_IF* in libssh).
 */
struct SftpFileMode
{
    enum : uint32_t
    {
        regular = 0100000,
        directory = 0040000,
        symlink = 0120000,
    };
};

/**
 * File attributes exchanged in SFTP messages, in either direction.
 *
 * Only the fields flagged in #flags (see SftpAttrFlags) carry meaningful values.
 */
struct SftpAttributes
{
    uint32_t flags{};
    uint64_t size{};
    uint32_t uid{};
    uint32_t gid{};
    uint32_t permissions{}; ///< permission bits, possibly combined with SftpFileMode bits
    uint32_t atime{};       ///< seconds since the epoch
    uint32_t mtime{};       ///< seconds since the epoch
};

} // namespace multipass
