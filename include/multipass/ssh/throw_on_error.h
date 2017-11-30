/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#ifndef MULTIPASS_SSH_THROW_ON_ERROR_H
#define MULTIPASS_SSH_THROW_ON_ERROR_H

#include <libssh/libssh.h>

#include <stdexcept>
#include <string>

namespace multipass
{
namespace SSH
{
template <typename Callable, typename Handle, typename... Args>
static void throw_on_error(Callable&& f, Handle&& h, Args&&... args)
{
    auto ret = f(h.get(), std::forward<Args>(args)...);
    if (ret != SSH_OK)
        throw std::runtime_error(std::string("ssh: ") + std::string(ssh_get_error(h.get())));
}
}
}
#endif // MULTIPASS_SSH_THROW_ON_ERROR_H
