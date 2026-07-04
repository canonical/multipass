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

#include <multipass/sshfs_mount/sftp_message.h>

#include <memory>

struct sftp_client_message_struct;

namespace multipass
{

/**
 * A concrete SftpMessage backed by a raw libssh message
 *
 * @copydoc SftpMessage
 */
class PlainSftpMessage final : public SftpMessage
{
public:
    ~PlainSftpMessage() override;

    /**
     * C'tor taking ownership of a raw libssh SFTP message
     *
     * @param message A reference to the the raw libssh SFTP message whose ownership is to be
     * adopted by this PlainSftpMessage
     */
    explicit PlainSftpMessage(sftp_client_message_struct& message) noexcept;
    PlainSftpMessage(const PlainSftpMessage&) = delete;
    PlainSftpMessage& operator=(const PlainSftpMessage&) = delete;

    // TODO@sftp Make class final before enabling these
    PlainSftpMessage(PlainSftpMessage&&) = delete;
    PlainSftpMessage& operator=(PlainSftpMessage&&) = delete;

    /**
     * @copydoc SftpMessage::type
     */
    SftpMessageType type() const noexcept override;

    /**
     * @copydoc SftpMessage::filename
     */
    std::string_view filename() const noexcept override;

    /**
     * @copydoc SftpMessage::data
     */
    std::string_view data() const noexcept override;

    /**
     * @copydoc SftpMessage::submessage
     */
    std::optional<std::string_view> submessage() const noexcept override;

    /**
     * @copydoc SftpMessage::flags
     */
    uint32_t flags() const noexcept override;

    /**
     * @copydoc SftpMessage::offset
     */
    uint64_t offset() const noexcept override;

    /**
     * @copydoc SftpMessage::length
     */
    uint32_t length() const noexcept override;

    /**
     * @copydoc SftpMessage::attributes
     */
    std::optional<SftpAttributes> attributes() const override;

    /**
     * @copydoc SftpMessage::handle
     */
    void* handle() const noexcept override;

    /**
     * @copydoc SftpMessage::remove_handle
     */
    void remove_handle() noexcept override;

    using SftpMessage::reply_status;

    /**
     * @copydoc SftpMessage::reply_status(SftpStatus, const std::string&)
     */
    bool reply_status(SftpStatus status, const std::string& message) override;

    /**
     * @copydoc SftpMessage::reply_attributes
     */
    bool reply_attributes(const SftpAttributes& attributes) override;

    /**
     * @copydoc SftpMessage::reply_data
     */
    bool reply_data(const void* data, size_t len) override;

    /**
     * @copydoc SftpMessage::reply_handle
     */
    bool reply_handle(void* id) override;

private:
    struct RawMsgDeleter
    {
        void operator()(sftp_client_message_struct* message) const noexcept;
    };
    using RawSftpMsgUptr = std::unique_ptr<sftp_client_message_struct, RawMsgDeleter>;

    RawSftpMsgUptr message;
};

} // namespace multipass
