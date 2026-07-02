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

#include <multipass/sshfs_mount/sftp_client_message.h>

#include <memory>

struct sftp_client_message_struct;

namespace multipass
{

/**
 * A concrete SftpMessage backed by a raw libssh message
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

private:
    struct RawMsgDeleter
    {
        void operator()(sftp_client_message_struct* message) const noexcept;
    };
    using RawSftpMsgUptr = std::unique_ptr<sftp_client_message_struct, RawMsgDeleter>;

    RawSftpMsgUptr message;
};

} // namespace multipass
