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
#include <iostream>

#include <memory>
#include <multipass/logging/level.h>
#include <multipass/logging/logger.h>
#include <rust/cxx.h>

#include <fmt/std.h>   // standard library formatters
#include <fmt/xchar.h> // char-type agnostic formatting

namespace multipass
{
namespace logging
{

/**
 * Log a preformatted message.
 *
 * It's safe to use this function with non-NUL terminated strings.
 *
 * @param [in] level Log level
 * @param [in] category Log category
 * @param [in] message The message
 */
void log_message(Level level, std::string_view category, std::string_view message);
void set_logger(std::shared_ptr<Logger> logger);
Level get_logging_level();
Logger* get_logger(); // for tests, don't rely on it lasting
namespace rust
{
void log_message(Level level, ::rust::String category, ::rust::String message);
class Base
{
public:
    virtual ~Base() = default;
    virtual void virt_func()
    {
        std::cout << "\nBase\n";
    }
    void base_func()
    {
        std::cout << "\nBase func\n";
    }
};
class Derived : public Base
{
public:
    ~Derived() override = default;
    void virt_func() override
    {
        std::cout << "\nDerived\n";
    }
};
inline std::unique_ptr<Base> get_base()
{
    return std::make_unique<Derived>();
}
} // namespace rust
/**
 * Log with formatting support
 *
 *
 * @tparam Arg0 Type of the first format argument
 * @tparam Args Type of the rest of the format arguments
 * @param [in] level Log level
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args The format arguments
 */
template <typename... Args>
constexpr void log(Level level,
                   std::string_view category,
                   fmt::format_string<Args...> fmt,
                   Args&&... args)
{
    const auto formatted_log_msg = fmt::format(fmt, std::forward<Args>(args)...);
    logging::log_message(level, category, formatted_log_msg);
}

/**
 * Log function with templated log level.
 *
 * @tparam level Log level
 * @tparam Args Format argument types
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args Format arguments, if any
 */
template <Level level, typename... Args>
constexpr void log(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log(level, category, fmt, std::forward<Args>(args)...);
}

/**
 * Log an error message.
 *
 * @tparam Args Format argument types
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args Format arguments
 */
template <typename... Args>
constexpr void error(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::error>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a warning message.
 *
 * @tparam Args Format argument types
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args Format arguments
 */
template <typename... Args>
constexpr void warn(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::warning>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a info message.
 *
 * @tparam Args Format argument types
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args Format arguments
 */
template <typename... Args>
constexpr void info(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::info>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a debug message.
 *
 * @tparam Args Format argument types
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args Format arguments
 */
template <typename... Args>
constexpr void debug(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::debug>(category, fmt, std::forward<Args>(args)...);
}

/**
 * Log a trace message.
 *
 * @tparam Args Format argument types
 * @param [in] category Log category
 * @param [in] fmt Format string
 * @param [in] args Format arguments
 */
template <typename... Args>
constexpr void trace(std::string_view category, fmt::format_string<Args...> fmt, Args&&... args)
{
    logging::log<Level::trace>(category, fmt, std::forward<Args>(args)...);
}

} // namespace logging
} // namespace multipass
