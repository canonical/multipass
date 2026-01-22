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
}

// using a struct and deduction guide to accommodate the default argument after the parameter pack
template <typename... Args>
struct log_location
{
    log_location(Level level,
                 std::string_view category,
                 fmt::format_string<Args...> fmt,
                 Args&&... args,
                 std::source_location location = std::source_location::current())
    {
        log(level,
            category,
            "{}:{} {}(): {}",
            detail::extract_filename(location.file_name()),
            location.line(),
            location.function_name(),
            fmt::format(fmt, std::forward<Args>(args)...));
    }
};

template <typename... Args>
log_location(Level, std::string_view, fmt::format_string<Args...>, Args&&...)
    -> log_location<Args...>;

template <typename... Args>
struct trace_location
{
    trace_location(std::string_view category,
                   fmt::format_string<Args...> fmt,
                   Args&&... args,
                   std::source_location location = std::source_location::current())
    {
        log_location<Args...>(Level::trace, category, fmt, std::forward<Args>(args)..., location);
    }
};

template <typename... Args>
trace_location(std::string_view, fmt::format_string<Args...>, Args&&...) -> trace_location<Args...>;

template <typename... Args>
struct debug_location
{
    debug_location(std::string_view category,
                   fmt::format_string<Args...> fmt,
                   Args&&... args,
                   std::source_location location = std::source_location::current())
    {
        log_location<Args...>(Level::debug, category, fmt, std::forward<Args>(args)..., location);
    }
};

template <typename... Args>
debug_location(std::string_view, fmt::format_string<Args...>, Args&&...) -> debug_location<Args...>;

} // namespace logging
} // namespace multipass
