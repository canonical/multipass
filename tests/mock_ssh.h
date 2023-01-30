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

#ifndef MULTIPASS_MOCK_SSH_H
#define MULTIPASS_MOCK_SSH_H

#include <premock.hpp>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

DECL_MOCK(ssh_new);
DECL_MOCK(ssh_connect);
DECL_MOCK(ssh_is_connected);
DECL_MOCK(ssh_options_set);
DECL_MOCK(ssh_userauth_publickey);
DECL_MOCK(ssh_channel_is_eof);
DECL_MOCK(ssh_channel_is_closed);
DECL_MOCK(ssh_channel_is_open);
DECL_MOCK(ssh_channel_new);
DECL_MOCK(ssh_channel_open_session);
DECL_MOCK(ssh_channel_request_exec);
DECL_MOCK(ssh_channel_read_timeout);
DECL_MOCK(ssh_channel_get_exit_status);
DECL_MOCK(ssh_event_dopoll);
DECL_MOCK(ssh_add_channel_callbacks);
DECL_MOCK(ssh_get_error);

#endif // MULTIPASS_MOCK_SSH_H
