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

#ifndef MULTIPASS_LEVEL_H
#define MULTIPASS_LEVEL_H

#include <multipass/logging/cstring.h>

namespace multipass
{
namespace logging
{

/**
 * The level of a log entry, in decreasing order of severity.
 */
enum class Level : int
{
    error = 0,   /**< Indicates a failure that prevents the intended operation from being accomplished in its entirety.
                      If there is a corresponding CLI command, it should exit with an error code. */
    warning = 1, /**< Indicates an event or fact that might not correspond to the users' intentions/desires/beliefs, or
                      a problem that is light enough that it does not prevent main goals from being accomplished.
                      If there is a corresponding CLI command, it should exit with a success code */
    info = 2,    /**< Indicates information that may be useful for the user to know, learn, etc. */
    debug = 3,   /**< Indicates information that is useful for developers and troubleshooting */
    trace = 4    /**< Indicates information that may be helpful for debugging but which would clutter logs unreasonably
                      if enabled by default */
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
