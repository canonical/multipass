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

#ifndef MULTIPASS_TOP_CATCH_ALL_H
#define MULTIPASS_TOP_CATCH_ALL_H

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <functional>
#include <type_traits>

namespace multipass
{
namespace detail
{
void error(const multipass::logging::CString& log_category,
           const std::exception& e);                         // not noexcept because logging isn't
void error(const multipass::logging::CString& log_category); // not noexcept because logging isn't
} // namespace detail

/**
 * Call a non-void function within a try-catch, catching and logging anything that it throws.
 *
 * @tparam T The type of the value that is used to initialize the return value when an exception is caught
 * @tparam Fun The type of the callable f. It must be callable and return non-void.
 * @tparam Args The types of f's arguments
 * @param log_category The category to use when logging exceptions
 * @param fallback_return The value to return value when an exception is caught
 * @param f The non-void function to protect with a catch-all
 * @param args The arguments to pass to the function f
 * @return The result of f when no exception is thrown, fallback_return otherwise
 * @note This function will call `terminate()` if logging itself throws (all bets are off at that point). That
 * corresponds to the usual `noexcept` guarantees (no exception or program terminated).
 */
template <typename T, typename Fun, typename... Args> // Fun needs to return non-void
auto top_catch_all(const logging::CString& log_category, T&& fallback_return, Fun&& f, Args&&... args) noexcept
    -> std::invoke_result_t<Fun, Args...>; // logging can throw, but we want to std::terminate in that case

/**
 * Call a void function within a try-catch, catching and logging anything that it throws.
 *
 * @tparam Fun The type of the callable f. It must be callable and return void.
 * @tparam Args The types of f's arguments
 * @param log_category The category to use when logging exceptions
 * @param f The non-void function to protect with a catch-all
 * @param args The arguments to pass to the function f
 * @note This function will call `terminate()` if logging itself throws (all bets are off at that point). That
 * corresponds to the usual `noexcept` guarantees (no exception or program terminated).
 */
template <typename Fun, typename... Args> // Fun needs to return void
void top_catch_all(const logging::CString& log_category, Fun&& f,
                   Args&&... args) noexcept; // logging can throw, but we want to std::terminate in that case
} // namespace multipass

inline void multipass::detail::error(const multipass::logging::CString& log_category, const std::exception& e)
{
    namespace mpl = multipass::logging;
    mpl::log(mpl::Level::error, log_category, fmt::format("Caught an unhandled exception: {}", e.what()));
}

inline void multipass::detail::error(const multipass::logging::CString& log_category)
{
    namespace mpl = multipass::logging;
    mpl::log(mpl::Level::error, log_category, "Caught an unknown exception");
}

template <typename T, typename Fun, typename... Args>
inline auto multipass::top_catch_all(const logging::CString& log_category, T&& fallback_return, Fun&& f,
                                     Args&&... args) noexcept -> std::invoke_result_t<Fun, Args...>
{
    try
    {
        return std::invoke(std::forward<Fun>(f), std::forward<Args>(args)...);
    }
    catch (const std::exception& e)
    {
        detail::error(log_category, e);
    }
    catch (...)
    {
        detail::error(log_category);
    }

    return std::forward<decltype(fallback_return)>(fallback_return);
}

template <typename Fun, typename... Args>
inline void multipass::top_catch_all(const logging::CString& log_category, Fun&& f, Args&&... args) noexcept
{
    try
    {
        std::invoke(std::forward<Fun>(f), std::forward<Args>(args)...);
    }
    catch (const std::exception& e)
    {
        detail::error(log_category, e);
    }
    catch (...)
    {
        detail::error(log_category);
    }
}

#endif // MULTIPASS_TOP_CATCH_ALL_H
