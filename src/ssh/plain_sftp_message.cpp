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

void mp::PlainSftpMessage::RawMsgDeleter::operator()(sftp_client_message_struct* msg) const noexcept
{
    sftp_client_message_free(msg);
}
