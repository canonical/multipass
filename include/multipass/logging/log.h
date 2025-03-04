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
 * via presence of a format argument. This is the reason why the 0th
 * argument is taken explicitly. The overload resolution rules of C++
 * makes it complicated to make it work reliably in the codebase, so
 * the code relies on explicity here.
 *
 * @ref https://en.cppreference.com/w/cpp/language/overload_resolution#Best_viable_function
 *
 * @tparam Arg0 Type of the first format argument
 * @tparam Args Type of the rest of the format arguments
 * @param level Log level
 * @param category Log category
 * @param fmt Format string
 * @param arg0 The first format argument
 * @param args Rest of the format arguments
 */
template <typename Arg0, typename... Args>
constexpr void
log(Level level, const std::string& category, fmt::format_string<Arg0, Args...> fmt, Arg0&& arg0, Args&&... args)
{
    const auto formatted_log_msg = fmt::format(fmt, std::forward<Arg0>(arg0), std::forward<Args>(args)...);
    log(level, category, formatted_log_msg);
}

void log(Level level, CString category, CString message);
void set_logger(std::shared_ptr<Logger> logger);
Level get_logging_level();
Logger* get_logger(); // for tests, don't rely on it lasting
} // namespace logging
} // namespace multipass
#endif // MULTIPASS_LOG_H
