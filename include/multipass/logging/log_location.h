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

#include <multipass/logging/log.h>

#include <source_location>
#include <string_view>

namespace multipass
{
namespace logging
{
namespace detail
{
std::string_view extract_filename(std::string_view path);

template <typename T>
struct with_source_location
{
    template <std::convertible_to<T> U>
    with_source_location(U&& val, std::source_location loc = std::source_location::current())
        : value(std::forward<U>(val)), location(loc)
    {
    }

    T value;
    std::source_location location;
};
} // namespace detail

template <typename... Args>
void log_location(Level level,
                  detail::with_source_location<std::string_view> category,
                  fmt::format_string<Args...> fmt,
                  Args&&... args)
{
    const auto& location = category.location;
    log(level,
        category.value,
        "{}:{}, `{}`: {}",
        detail::extract_filename(location.file_name()),
        location.line(),
        location.function_name(),
        fmt::format(fmt, std::forward<Args>(args)...));
}

template <Level level, typename... Args>
void log_location(detail::with_source_location<std::string_view> category,
                  fmt::format_string<Args...> fmt,
                  Args&&... args)
{
    log_location(level, category, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void trace_location(detail::with_source_location<std::string_view> category,
                    fmt::format_string<Args...> fmt,
                    Args&&... args)
{
    log_location(Level::trace, category, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void debug_location(detail::with_source_location<std::string_view> category,
                    fmt::format_string<Args...> fmt,
                    Args&&... args)
{
    log_location(Level::debug, category, fmt, std::forward<Args>(args)...);
}

} // namespace logging
} // namespace multipass
