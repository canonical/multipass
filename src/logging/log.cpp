/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/logging/log.h>

#include <shared_mutex>

namespace mpl = multipass::logging;

namespace
{
std::shared_timed_mutex mutex;
std::shared_ptr<multipass::logging::Logger> global_logger;
} // namespace

void mpl::log(Level level, CString category, CString message)
{
    std::shared_lock<decltype(mutex)> lock{mutex};
    if (global_logger)
        global_logger->log(level, category, message);
}

void mpl::set_logger(std::shared_ptr<Logger> logger)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    global_logger = logger;
}
