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

#pragma once

#include <multipass/sshfs_mount/sftp_protocol.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace multipass
{

/**
 * A unit of communication exchange between an SFTP client and an SFTP server, in either direction.
 */
class SftpMessage
{
public:
    virtual ~SftpMessage() = default;

    // No copies
    SftpMessage(const SftpMessage&) = delete;
    SftpMessage& operator=(const SftpMessage&) = delete;

    /**
     * @return The type of request this message conveys.
     */
    virtual SftpMessageType type() const noexcept = 0;

    /**
     * @return The path argument of the request (e.g. the file to open, the directory to create;
     * for symlink requests, the link target); empty if the request carries none.
     */
    virtual std::string_view filename() const noexcept = 0;

    /**
     * @return The data argument of the request: the payload of a write request or the second path
     * of two-path requests (e.g. the new path of a rename, the link path of a symlink); empty if
     * the request carries none.
     */
    virtual std::string_view data() const noexcept = 0;

    /**
     * @return The submessage name of an extended request (e.g. "posix-rename@openssh.com");
     * nullopt if absent.
     */
    virtual std::optional<std::string_view> submessage() const noexcept = 0;

    /**
     * @return The access flags of an open request (a combination of sftp_open_flags).
     */
    virtual uint32_t flags() const noexcept = 0;

    /**
     * @return The file offset of a read/write request.
     */
    virtual uint64_t offset() const noexcept = 0;

    /**
     * @return The number of bytes requested by a read request.
     */
    virtual uint32_t length() const noexcept = 0;

protected:
    SftpMessage() = default;
};

} // namespace multipass
