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

#pragma once

#include <libssh/libssh.h>

namespace multipass
{

/**
 * Forward-declarable wrapper for a platform-native socket descriptor.
 * Implicitly converts to and from the underlying @c socket_t,
 * whose definition we reuse from libssh (@c int on POSIX, @c SOCKET on Windows).
 */
struct Socket
{
    /* implicit */ Socket(socket_t fd);
    operator socket_t() const;

    socket_t fd;
};

} // namespace multipass

inline /* implicit */ multipass::Socket::Socket(socket_t fd) : fd{fd}
{
}

inline multipass::Socket::operator socket_t() const
{
    return fd;
}
