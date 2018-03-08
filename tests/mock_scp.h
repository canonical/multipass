/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_SCP_H
#define MULTIPASS_MOCK_SCP_H

#include <premock.hpp>

#include <libssh/libssh.h>

DECL_MOCK(ssh_scp_new);
DECL_MOCK(ssh_scp_free);
DECL_MOCK(ssh_scp_init);
DECL_MOCK(ssh_scp_push_file);
DECL_MOCK(ssh_scp_write);
DECL_MOCK(ssh_scp_pull_request);
DECL_MOCK(ssh_scp_request_get_size);
DECL_MOCK(ssh_scp_request_get_filename);
DECL_MOCK(ssh_scp_accept_request);
DECL_MOCK(ssh_scp_read);
DECL_MOCK(ssh_scp_close);

#endif // MULTIPASS_MOCK_SCP_H
