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

#include <multipass/ssh/plain_sftp_message.h>

#include <libssh/sftp.h>

namespace mp = multipass;

mp::PlainSftpMessage::~PlainSftpMessage() = default;

mp::PlainSftpMessage::PlainSftpMessage(sftp_client_message_struct& message) noexcept
    : message{&message}
{
}

mp::SftpMessageType mp::PlainSftpMessage::type() const noexcept
{
    return static_cast<SftpMessageType>(sftp_client_message_get_type(message.get()));
}

std::string_view mp::PlainSftpMessage::filename() const noexcept
{
    const auto* raw = sftp_client_message_get_filename(message.get());
    return raw ? std::string_view{raw} : std::string_view{};
}

std::string_view mp::PlainSftpMessage::data() const noexcept
{
    if (auto* raw = message->data) // ssh_string carries a length, so this is binary safe
        return {ssh_string_get_char(raw), ssh_string_len(raw)};

    return {};
}

std::optional<std::string_view> mp::PlainSftpMessage::submessage() const noexcept
{
    const auto* raw = sftp_client_message_get_submessage(message.get());
    return raw ? std::optional<std::string_view>{raw} : std::nullopt;
}

uint32_t mp::PlainSftpMessage::flags() const noexcept
{
    return sftp_client_message_get_flags(message.get());
}

uint64_t mp::PlainSftpMessage::offset() const noexcept
{
    return message->offset;
}

uint32_t mp::PlainSftpMessage::length() const noexcept
{
    return message->len;
}

std::optional<mp::SftpAttributes> mp::PlainSftpMessage::attributes() const
{
    const auto* raw = message->attr;
    if (!raw)
        return std::nullopt;

    return SftpAttributes{raw->flags,
                          raw->size,
                          raw->uid,
                          raw->gid,
                          raw->permissions,
                          raw->atime,
                          raw->mtime};
}

void* mp::PlainSftpMessage::handle() const noexcept
{
    return sftp_handle(message->sftp, message->handle);
}

void mp::PlainSftpMessage::RawMsgDeleter::operator()(sftp_client_message_struct* msg) const noexcept
{
    sftp_client_message_free(msg);
}
