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
template <typename Ret, typename Fun, typename... Args> // Fun needs to return int
Ret top_catch_all(const logging::CString& log_category, Ret&& fallback_return, Fun&& f,
                  Args&&... args); // not noexcept because logging isn't
}

template <typename Ret, typename Fun, typename... Args>
inline Ret multipass::top_catch_all(const logging::CString& log_category, Ret&& fallback_return, Fun&& f,
                                    Args&&... args)
{
    namespace mpl = multipass::logging;
    try
    {
        return std::invoke(std::forward<Fun>(f), std::forward<Args>(args)...);
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::error, log_category, fmt::format("Caught an unhandled exception: {}", e.what()));
    }
    catch (...)
    {
        mpl::log(mpl::Level::error, log_category, "Caught an unknown exception");
    }

    return std::forward<decltype(fallback_return)>(fallback_return);
}

#endif // MULTIPASS_TOP_CATCH_ALL_H
