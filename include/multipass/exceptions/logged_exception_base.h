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

#ifndef MULTIPASS_LOGGED_EXCEPTION_BASE_H
#define MULTIPASS_LOGGED_EXCEPTION_BASE_H

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/logging/log.h>

#include <stdexcept>
#include <string_view>

namespace multipass
{

/**
 * A templated exception base type that has formatting and logging by default.
 *
 * The produced exception message would be logged automatically.
 *
 * This class is intended to be used as a base class to user-defined
 * exception types. The user-defined exception type is either expected
 * to inherit the constructors of LoggedExceptionBase, or call them
 * explicitly.
 *
 * @tparam BaseExceptionType The exception type. Defaults to `std::runtime_error`.
 * The exception type must be constructible with a std::string, implement a what() method
 * that returns a type convertible to std::string_view, and must derive from std::exception.
 */
template <multipass::logging::Level level = multipass::logging::Level::error,
          typename BaseExceptionType = std::runtime_error>
struct LoggedExceptionBase : public FormattedExceptionBase<BaseExceptionType>
{
    /**
     * Format @p args and log the resulting string
     * upon construction
     *
     * @tparam Args Format argument types
     *
     * @param [in] category Log category
     * @param [in] args Format arguments
     */
    template <typename... Args>
    LoggedExceptionBase(std::string_view category, Args&&... args)
        : FormattedExceptionBase<BaseExceptionType>(std::forward<Args>(args)...)
    {
        static_assert(sizeof...(Args) > 0,
                      "LoggedExceptionBase requires at least the format argument in addition to the category.");
        try
        {
            multipass::logging::log(level, category, this->what());
        }
        catch (...)
        {
        }
    }
};

} // namespace multipass

#endif // MULTIPASS_LOGGED_EXCEPTION_BASE_H
