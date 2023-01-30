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

#include "mock_sftp.h"
extern "C"
{
    IMPL_MOCK_DEFAULT(1, sftp_new);
    IMPL_MOCK_DEFAULT(1, sftp_free);
    IMPL_MOCK_DEFAULT(1, sftp_init);
    IMPL_MOCK_DEFAULT(4, sftp_open);
    IMPL_MOCK_DEFAULT(3, sftp_write);
    IMPL_MOCK_DEFAULT(3, sftp_read);
    IMPL_MOCK_DEFAULT(1, sftp_get_error);
    IMPL_MOCK_DEFAULT(1, sftp_close);
    IMPL_MOCK_DEFAULT(2, sftp_stat);
    IMPL_MOCK_DEFAULT(2, sftp_lstat);
    IMPL_MOCK_DEFAULT(2, sftp_opendir);
    IMPL_MOCK_DEFAULT(2, sftp_readdir);
    IMPL_MOCK_DEFAULT(2, sftp_readlink);
    IMPL_MOCK_DEFAULT(3, sftp_mkdir);
    IMPL_MOCK_DEFAULT(3, sftp_symlink);
    IMPL_MOCK_DEFAULT(2, sftp_unlink);
    IMPL_MOCK_DEFAULT(3, sftp_setstat);
    IMPL_MOCK_DEFAULT(1, sftp_dir_eof);
    IMPL_MOCK_DEFAULT(3, sftp_chmod);
}
