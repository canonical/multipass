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

#include <fmt/format.h>

#include <stdexcept>
#include <string>
#include <type_traits>

namespace multipass
{

/**
 * A templated exception base type that has formatting by default.
 *
 * This class is intended to be used as a base class to user-defined
 * exception types. The user-defined exception type is either expected
 * to inherit the constructors of FormattedExceptionBase, or call them
 * explicitly.
 *
 * @tparam BaseExceptionType The exception type. Defaults to `std::runtime_error`.
 * The exception type must be constructible with a std::string, and must derive from
 * std::exception.
 */
template <typename BaseExceptionType = std::runtime_error>
struct FormattedExceptionBase : public BaseExceptionType
{
    // These are here for giving nice and understandable compilation errors
    // instead of a 1000 page of cryptic template wall of error message.
    static_assert(std::is_constructible<BaseExceptionType, std::string>::value ||
                      std::is_constructible<BaseExceptionType, std::error_code, std::string>::value,
                  "BaseExceptionType must either be constructible with (std::string) or "
                  "(std::error_code, std::string).");
    static_assert(std::is_base_of<std::exception, BaseExceptionType>::value,
                  "BaseExceptionType must derive from std::exception");

    /**
     * Produces a formatted string using fmt and args.
     *
     * Possible format exceptions are handled gracefully.
     *
     * @tparam Args Format argument types
     *
     * @param [in] fmt Format string
     * @param [in] args Format arguments
     */
    template <typename... Args>
    FormattedExceptionBase(fmt::format_string<Args...> fmt, Args&&... args)
        : BaseExceptionType(failsafe_format(fmt, std::forward<Args>(args)...))
    {
    }

    /**
     * Produces a formatted string using fmt and args.
     *
     * Possible format exceptions are handled gracefully.
     *
     * @tparam Args Format argument types
     *
     * @param [in] ec Error code
     * @param [in] fmt Format string
     * @param [in] args Format arguments
     */
    template <typename... Args>
    FormattedExceptionBase(std::error_code ec, fmt::format_string<Args...> fmt, Args&&... args)
        : BaseExceptionType(ec, failsafe_format(fmt, std::forward<Args>(args)...))
    {
    }

private:
    /**
     * Exception catcher for fmt::format.
     *
     * Since this class is an exception class itself, it cannot simply throw
     * exceptions on the constructor, otherwise it'll lead to `std::terminate()`
     * and we definitely don't want to terminate just because of a recoverable
     * format error.
     *
     * Instead, the code catches the exception and returns a string that indicate
     * that there was a format error with the given format string, so it can be
     * traced and fixed.
     *
     * The function is not marked as `noexcept` since in theory the std::string
     * constructor could throw, and we're not handling that. The typical base exception
     * types to this class do not give that guarantee either (.e.g, std::runtime_error)
     * so the code does not bother.
     *
     * @tparam Args Format argument types
     * @param [in] fmt Format string
     * @param [in] args Format arguments
     *
     * @return std::string Format result
     */
    template <typename... Args>
    static std::string failsafe_format(fmt::format_string<Args...> fmt, Args&&... args)
    try
    {
        return fmt::format(fmt, std::forward<Args>(args)...);
    }
    catch (const std::exception& e)
    {
        std::string msg{"[Error while formatting the exception string]"};
        msg += "\nFormat string: `";
        msg += fmt.get().data();
        msg += "`\nFormat error: `";
        msg += e.what();
        msg += '`';
        return msg;
    }
    catch (...)
    {
        // In an unlikely event of fmt::format throwing a non-exception type.
        std::string msg{"[Error while formatting the exception string]"};
        msg += "\nFormat string: `";
        msg += fmt.get().data();
        msg += '`';
        return msg;
    }
};

} // namespace multipass
