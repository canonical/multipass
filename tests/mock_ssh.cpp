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

#include "mock_ssh.h"
extern "C"
{
    IMPL_MOCK_DEFAULT(0, ssh_new);
    IMPL_MOCK_DEFAULT(1, ssh_connect);
    IMPL_MOCK_DEFAULT(1, ssh_is_connected);
    IMPL_MOCK_DEFAULT(3, ssh_options_set);
    IMPL_MOCK_DEFAULT(3, ssh_userauth_publickey);
    IMPL_MOCK_DEFAULT(1, ssh_channel_is_eof);
    IMPL_MOCK_DEFAULT(1, ssh_channel_is_closed);
    IMPL_MOCK_DEFAULT(1, ssh_channel_is_open);
    IMPL_MOCK_DEFAULT(1, ssh_channel_new);
    IMPL_MOCK_DEFAULT(1, ssh_channel_open_session);
    IMPL_MOCK_DEFAULT(2, ssh_channel_request_exec);
    IMPL_MOCK_DEFAULT(5, ssh_channel_read_timeout);
    IMPL_MOCK_DEFAULT(1, ssh_channel_get_exit_status);
    IMPL_MOCK_DEFAULT(2, ssh_event_dopoll);
    IMPL_MOCK_DEFAULT(2, ssh_add_channel_callbacks);
    IMPL_MOCK_DEFAULT(1, ssh_get_error);
}
