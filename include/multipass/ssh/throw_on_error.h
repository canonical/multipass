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

#ifndef MULTIPASS_SSH_THROW_ON_ERROR_H
#define MULTIPASS_SSH_THROW_ON_ERROR_H

#include <libssh/libssh.h>

#include <fmt/format.h>

#include <multipass/exceptions/ssh_exception.h>

#include <string>
#include <type_traits>

namespace multipass
{
namespace SSH
{
template <typename Handle, typename Callable, typename... Args>
void throw_on_error(Handle&& h, ssh_session session, const char* error_msg, Callable&& f, Args&&... args)
{
    const auto ret = f(h.get(), std::forward<Args>(args)...);
    if (ret != SSH_OK)
    {
        throw SSHException(fmt::format("{}: '{}'", error_msg, ssh_get_error(session)));
    }
}

template <typename Handle, typename Callable, typename... Args>
void throw_on_error(Handle&& h, const char* error_msg, Callable&& f, Args&&... args)
{
    // Ensure that the handle type is appropriate for ssh_get_error
    using HandleType = typename std::remove_reference<Handle>::type;
    using HandlePointerType = decltype(std::declval<HandleType>().get());
    static_assert(std::is_same<ssh_session, HandlePointerType>::value, "ssh_get_error needs an ssh_session");

    throw_on_error(h, h.get(), error_msg, f, std::forward<Args>(args)...);
}
} // namespace SSH
} // namespace multipass
#endif // MULTIPASS_SSH_THROW_ON_ERROR_H
