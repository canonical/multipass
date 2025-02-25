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

#ifndef MULTIPASS_LOG_H
#define MULTIPASS_LOG_H

#include <multipass/logging/cstring.h>
#include <multipass/logging/level.h>
#include <multipass/logging/logger.h>

#include <fmt/std.h>   // standard library formatters
#include <fmt/xchar.h> // char-type agnostic formatting

namespace multipass
{
namespace logging
{

/**
 * Log with formatting support
 *
 * The old (legacy) log function and this overload are distinguished
 * via presence of a format argument. When a format argument is absent
 * both the old function and the new one are valid candidates for a
 * `log(Level::debug, "test", "test")` call. The ambiguity is resolved
 * by the fact that a non-templated overload is a better fit.
 *
 * @ref https://en.cppreference.com/w/cpp/language/overload_resolution#Best_viable_function
 *
 * @tparam Args Type of the format arguments
 * @param level Log level
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
constexpr void log(Level level, const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    const auto formatted_log_msg = fmt::format(fmt, std::forward<Args>(args)...);
    // CString{} is necessary here. Without it, this function would infinitely recurse.
    // The reason "why" is: the non-templated log() overload requires an implicit
    // const std::string& -> CString conversion for the category argument, which
    // makes it a "worse" overload than this templated function. We do the conversion
    // here in order to prevent that.
    log(level, CString{category}, formatted_log_msg);
}

void log(Level level, CString category, CString message);
void set_logger(std::shared_ptr<Logger> logger);
Level get_logging_level();
Logger* get_logger(); // for tests, don't rely on it lasting
} // namespace logging
} // namespace multipass
#endif // MULTIPASS_LOG_H
