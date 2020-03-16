/*
 * Copyright (C) 2020 Canonical, Ltd.
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

namespace multipass
{
template <typename F, typename... Args>                                      // F needs to return int
int top_catch_all(const logging::CString log_category, F f, Args&&... args); // not noexcept because logging isn't
}

template <typename F, typename... Args>
inline int multipass::top_catch_all(const logging::CString log_category, F f, Args&&... args)
{
    namespace mpl = multipass::logging;
    try
    {
        return f(std::forward<Args>(args)...);
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::error, log_category, fmt::format("Caught an unhandled exception: {}", e.what()));
        return EXIT_FAILURE;
    }
    catch (...)
    {
        mpl::log(mpl::Level::error, log_category, "Caught an unknown exception");
        return EXIT_FAILURE;
    }
}

#endif // MULTIPASS_TOP_CATCH_ALL_H
