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

#include <cassert>
#include <source_location>

namespace multipass
{
namespace logging
{
namespace detail
{

// Carefule with temporaries
constexpr std::string_view extract_filename(std::string_view path)
{
    assert(path.empty() || (*path.rbegin() != '/' && *path.rbegin() != '\\')); // no trailing slash

    auto pos = path.find_last_of("/\\");
    return pos == std::string_view::npos ? path : path.substr(pos + 1);
}

} // namespace detail

// using a struct and deduction guide to accommodate the default argument after the parameter pack
template <typename... Args>
struct trace_loc
{
    trace_loc(std::string_view category,
              fmt::format_string<Args...> fmt,
              Args&&... args,
              std::source_location location = std::source_location::current())
    {
        trace(category,
              "{}:{} {}(): {}",
              detail::extract_filename(location.file_name()),
              location.line(),
              location.function_name(),
              fmt::format(fmt, std::forward<Args>(args)...));
    }
};

template <typename... Args>
trace_loc(std::string_view, fmt::format_string<Args...>, Args&&...) -> trace_loc<Args...>;

} // namespace logging
} // namespace multipass
