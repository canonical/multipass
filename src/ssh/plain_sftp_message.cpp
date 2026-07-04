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

#include <cassert>
#include <limits>

namespace mp = multipass;

namespace
{
sftp_attributes_struct to_raw_attributes(const mp::SftpAttributes& attributes) noexcept
{
    sftp_attributes_struct raw{};
    raw.flags = attributes.flags;
    raw.size = attributes.size;
    raw.uid = attributes.uid;
    raw.gid = attributes.gid;
    raw.permissions = attributes.permissions;
    raw.atime = attributes.atime;
    raw.mtime = attributes.mtime;

    return raw;
}
} // namespace

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

std::optional<mp::SftpAttributes> mp::PlainSftpMessage::attributes() const noexcept
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

void mp::PlainSftpMessage::remove_handle() noexcept
{
    if (auto* id = handle())
        sftp_handle_remove(message->sftp, id);
}

bool mp::PlainSftpMessage::reply_status(SftpStatus status, const std::string& msg)
{
    // TODO@C++23 Convert cast to std::to_underlying
    return sftp_reply_status(message.get(), static_cast<uint32_t>(status), msg.c_str()) == SSH_OK;
}

bool mp::PlainSftpMessage::reply_attributes(const SftpAttributes& attributes)
{
    auto raw = to_raw_attributes(attributes);
    return sftp_reply_attr(message.get(), &raw) == SSH_OK;
}

bool mp::PlainSftpMessage::reply_data(const void* data, size_t len)
{
    assert(len <= static_cast<size_t>(std::numeric_limits<int>::max()) &&
           "data replies are bounded by packet size");
    return sftp_reply_data(message.get(), data, static_cast<int>(len)) == SSH_OK;
}

bool mp::PlainSftpMessage::reply_name(const std::string& name)
{
    return sftp_reply_name(message.get(), name.c_str(), nullptr) == SSH_OK;
}

bool mp::PlainSftpMessage::reply_handle(void* id)
{
    using Del = decltype([](ssh_string_struct* handle) noexcept { ssh_string_free(handle); });
    using RawHandleUptr = std::unique_ptr<ssh_string_struct, Del>;

    const RawHandleUptr raw_handle{sftp_handle_alloc(message->sftp, id)};
    if (!raw_handle)
        return false;

    return sftp_reply_handle(message.get(), raw_handle.get()) == SSH_OK;
}

bool mp::PlainSftpMessage::reply_names_add(const std::string& file,
                                           const std::string& longname,
                                           const SftpAttributes& attributes)
{
    auto raw = to_raw_attributes(attributes);
    return sftp_reply_names_add(message.get(), file.c_str(), longname.c_str(), &raw) == SSH_OK;
}

bool mp::PlainSftpMessage::reply_names()
{
    return sftp_reply_names(message.get()) == SSH_OK;
}

void mp::PlainSftpMessage::RawMsgDeleter::operator()(sftp_client_message_struct* msg) const noexcept
{
    sftp_client_message_free(msg);
}
