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

void log(Level level, CString category, CString message);
void set_logger(std::shared_ptr<Logger> logger);
Level get_logging_level();
Logger* get_logger(); // for tests, don't rely on it lasting

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
    logging::log(level, category, formatted_log_msg);
}

/**
 * Log function with templated log level.
 *
 * This function acts as an dispatch point for the templated
 * and non-templated log overloads, based on argument count.
 * We cannot simply use the templated overload since it requires
 * at least 1 arguments.
 *
 * @tparam level Log level
 * @tparam Args Format argument types
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments, if any
 */
template <Level level, typename... Args>
constexpr void log(const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    if constexpr (sizeof...(Args) > 0)
    {
        // Needs formatting
        logging::log(level, category, fmt, std::forward<Args>(args)...);
        return;
    }
    logging::log(level, category, CString{fmt.get().data()});
}

/**
 * Log an error message.
 *
 * @tparam Args Format argument types
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
constexpr void error(const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::error>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a warning message.
 *
 * @tparam Args List of types for format arguments
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
constexpr void warn(const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::warning>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a info message.
 *
 * @tparam Args List of types for format arguments
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
constexpr void info(const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::info>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a debug message.
 *
 * @tparam Args List of types for format arguments
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
constexpr void debug(const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::debug>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a trace message.
 *
 * @tparam Args List of types for format arguments
 * @param category Log category
 * @param fmt Format string
 * @param args Format arguments
 */
template <typename... Args>
constexpr void trace(const std::string& category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::trace>(category, fmt, std::forward<Args>(args)...);
}

} // namespace logging
} // namespace multipass
#endif // MULTIPASS_LOG_H
