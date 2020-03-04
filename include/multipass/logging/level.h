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

#ifndef MULTIPASS_LEVEL_H
#define MULTIPASS_LEVEL_H

#include <multipass/logging/cstring.h>

namespace multipass
{
namespace logging
{
enum class Level : int
{
    error = 0,
    warning = 1,
    info = 2,
    debug = 3,
    trace = 4
};

constexpr CString as_string(const Level& l) noexcept
{
    switch (l)
    {
    case Level::debug:
        return "debug";
    case Level::error:
        return "error";
    case Level::info:
        return "info";
    case Level::warning:
        return "warning";
    case Level::trace:
        return "trace";
    }
    return "unknown";
}

constexpr auto enum_type(Level e) noexcept
{
    return static_cast<std::underlying_type_t<Level>>(e);
}

constexpr Level level_from(std::underlying_type_t<Level> in)
{
    return static_cast<Level>(in);
}

constexpr bool operator<(Level a, Level b) noexcept
{
    return enum_type(a) < enum_type(b);
}

constexpr bool operator>(Level a, Level b) noexcept
{
    return enum_type(a) > enum_type(b);
}

constexpr bool operator<=(Level a, Level b) noexcept
{
    return enum_type(a) <= enum_type(b);
}

constexpr bool operator>=(Level a, Level b) noexcept
{
    return enum_type(a) >= enum_type(b);
}
} // namespace logging
} // namespace multipass

#endif // MULTIPASS_LEVEL_H
