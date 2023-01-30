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

#ifndef MULTIPASS_MOCK_SFTPSERVER_H
#define MULTIPASS_MOCK_SFTPSERVER_H

#include <premock.hpp>

#include <libssh/sftp.h>

DECL_MOCK(sftp_server_new);
DECL_MOCK(sftp_server_init);
DECL_MOCK(sftp_reply_status);
DECL_MOCK(sftp_reply_attr);
DECL_MOCK(sftp_reply_data);
DECL_MOCK(sftp_reply_name);
DECL_MOCK(sftp_reply_names);
DECL_MOCK(sftp_reply_names_add);
DECL_MOCK(sftp_reply_handle);
DECL_MOCK(sftp_get_client_message);
DECL_MOCK(sftp_client_message_free);
DECL_MOCK(sftp_client_message_get_data);
DECL_MOCK(sftp_client_message_get_filename);
DECL_MOCK(sftp_handle);
DECL_MOCK(sftp_handle_alloc);
DECL_MOCK(sftp_handle_remove);

#endif // MULTIPASS_MOCK_SFTPSERVER_H
