/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

namespace multipass
{
namespace detail
{
void error(const multipass::logging::CString& log_category,
           const std::exception& e);                         // not noexcept because logging isn't
void error(const multipass::logging::CString& log_category); // not noexcept because logging isn't
} // namespace detail

/**
 * Call f within a try-catch, catching and logging anything that it throws.
 *
 * @tparam Fun The type of the callable f. It must be callable and return int.
 * @tparam Args The types of f's arguments
 * @param log_category The category to use when logging exceptions
 * @param f The int-returning function to protect with a catch-all
 * @param args The arguments to pass to the function f
 * @return The result of f when no exception is thrown, EXIT_FAILURE (from cstdlib) otherwise
 * TODO@ricab fix doc
 */
template <typename T, typename Fun, typename... Args> // Fun needs to return int
auto top_catch_all(const logging::CString& log_category, T&& fallback_return, Fun&& f,
                   Args&&... args); // not noexcept because logging isn't

template <typename Fun, typename... Args>
void top_catch_all(const logging::CString& log_category, Fun&& f,
                   Args&&... args); // not noexcept because logging isn't
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
inline auto multipass::top_catch_all(const logging::CString& log_category, T&& fallback_return, Fun&& f, Args&&... args)
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

    return std::invoke_result_t<Fun, Args...>{std::forward<decltype(fallback_return)>(fallback_return)};
}

template <typename Fun, typename... Args>
inline void multipass::top_catch_all(const logging::CString& log_category, Fun&& f,
                                     Args&&... args) // not noexcept because logging isn't
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
