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

#include "mock_sftpserver.h"
extern "C"
{
    IMPL_MOCK_DEFAULT(2, sftp_server_new);
    IMPL_MOCK_DEFAULT(1, sftp_server_init);
    IMPL_MOCK_DEFAULT(3, sftp_reply_status);
    IMPL_MOCK_DEFAULT(2, sftp_reply_attr);
    IMPL_MOCK_DEFAULT(3, sftp_reply_data);
    IMPL_MOCK_DEFAULT(3, sftp_reply_name);
    IMPL_MOCK_DEFAULT(1, sftp_reply_names);
    IMPL_MOCK_DEFAULT(4, sftp_reply_names_add);
    IMPL_MOCK_DEFAULT(2, sftp_reply_handle);
    IMPL_MOCK_DEFAULT(1, sftp_get_client_message);
    IMPL_MOCK_DEFAULT(1, sftp_client_message_free);
    IMPL_MOCK_DEFAULT(1, sftp_client_message_get_data);
    IMPL_MOCK_DEFAULT(1, sftp_client_message_get_filename);
    IMPL_MOCK_DEFAULT(2, sftp_handle);
    IMPL_MOCK_DEFAULT(2, sftp_handle_alloc);
    IMPL_MOCK_DEFAULT(2, sftp_handle_remove);
}
