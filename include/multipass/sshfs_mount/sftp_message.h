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
#include <string>
#include <string_view>

namespace multipass
{

/**
 * A unit of communication exchange between an SFTP client and an SFTP server, in either direction.
 *
 * Provides access to the client request along with methods to reply.
 */
class SftpMessage // TODO@sftp check if there is anythign unused at in the end
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

    /**
     * @return The file attributes carried by the request; nullopt if it carries none.
     */
    virtual std::optional<SftpAttributes> attributes() const = 0;

    /**
     * Resolve the protocol handle carried by the request (e.g. by read/write/close requests).
     *
     * @return The native handle that #reply_handle registered; nullptr if the request's handle is
     * absent or unknown.
     */
    virtual void* handle() const noexcept = 0;

    /**
     * Unregister the protocol handle carried by the request, undoing #reply_handle's registration
     * (e.g. when serving a close request). No effect if the handle is absent or unknown.
     */
    virtual void remove_handle() noexcept = 0;

    /**
     * Reply with a status code.
     *
     * @param status The status code to reply with.
     * @param message A human-readable complement to the status code.
     * @return True if the reply was successfully sent.
     */
    virtual bool reply_status(SftpStatus status, const std::string& message) = 0;

    /**
     * @copybrief reply_status(SftpStatus, const std::string&)
     * Overload replying with a status code alone.
     *
     * @param status The status code to reply with.
     * @return True if the reply was successfully sent.
     */
    bool reply_status(SftpStatus status)
    {
        return reply_status(status, {});
    }

    /**
     * Reply with file attributes (e.g. to stat requests).
     *
     * @param attributes Stat attributes to reply with
     * @return True if the reply was successfully sent.
     */
    virtual bool reply_attributes(const SftpAttributes& attributes) = 0;

    /**
     * Reply with raw data (i.e. to read requests).
     *
     * @param data The data to reply with.
     * @param len The number of bytes to reply with.
     * @return True if the reply was successfully sent.
     */
    virtual bool reply_data(const void* data, size_t len) = 0;

    /**
     * Register @p id and reply with a new protocol handle referring to it (e.g. to open/opendir
     * requests). The remote client is expected to carry that handle in follow-up requests,
     * where #handle resolves it back to @p id.
     *
     * @param id The native handle to register.
     * @return True if the reply was successfully sent; false if registration failed (e.g. @p id
     * was already registered) or sending failed, in which case nothing may have been sent and the
     * caller can still reply otherwise.
     */
    virtual bool reply_handle(void* id) = 0;

protected:
    SftpMessage() = default;
};

} // namespace multipass
