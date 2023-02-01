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

#ifndef MULTIPASS_MOCK_SFTP_H
#define MULTIPASS_MOCK_SFTP_H

#include <premock.hpp>

#include <libssh/sftp.h>

DECL_MOCK(sftp_new);
DECL_MOCK(sftp_free);
DECL_MOCK(sftp_init);
DECL_MOCK(sftp_open);
DECL_MOCK(sftp_write);
DECL_MOCK(sftp_read);
DECL_MOCK(sftp_get_error);
DECL_MOCK(sftp_close);
DECL_MOCK(sftp_stat);
DECL_MOCK(sftp_lstat);
DECL_MOCK(sftp_opendir);
DECL_MOCK(sftp_readdir);
DECL_MOCK(sftp_readlink);
DECL_MOCK(sftp_mkdir);
DECL_MOCK(sftp_symlink);
DECL_MOCK(sftp_unlink);
DECL_MOCK(sftp_setstat);
DECL_MOCK(sftp_dir_eof);
DECL_MOCK(sftp_chmod);

#endif // MULTIPASS_MOCK_SFTP_H
