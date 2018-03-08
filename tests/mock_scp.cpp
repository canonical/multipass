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

#include "mock_scp.h"
extern "C" {
IMPL_MOCK_DEFAULT(3, ssh_scp_new);
IMPL_MOCK_DEFAULT(1, ssh_scp_free);
IMPL_MOCK_DEFAULT(1, ssh_scp_init);
IMPL_MOCK_DEFAULT(4, ssh_scp_push_file);
IMPL_MOCK_DEFAULT(3, ssh_scp_write);
IMPL_MOCK_DEFAULT(1, ssh_scp_pull_request);
IMPL_MOCK_DEFAULT(1, ssh_scp_request_get_size);
IMPL_MOCK_DEFAULT(1, ssh_scp_request_get_filename);
IMPL_MOCK_DEFAULT(1, ssh_scp_accept_request);
IMPL_MOCK_DEFAULT(3, ssh_scp_read);
IMPL_MOCK_DEFAULT(1, ssh_scp_close);
}
