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

#include <multipass/logging/multiplexing_logger.h>

#include <algorithm>

namespace mpl = multipass::logging;

multipass::logging::MultiplexingLogger::MultiplexingLogger(UPtr system_logger)
    : Logger{system_logger->get_logging_level()}, system_logger{std::move(system_logger)}
{
}

void mpl::MultiplexingLogger::log(mpl::Level level, CString category, CString message) const
{
    std::shared_lock<decltype(mutex)> lock{mutex};
    system_logger->log(level, category, message);
    for (auto logger : loggers)
        logger->log(level, category, message);
}

void mpl::MultiplexingLogger::add_logger(const Logger* logger)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    loggers.push_back(logger);
}

void mpl::MultiplexingLogger::remove_logger(const Logger* logger)
{
    std::lock_guard<decltype(mutex)> lock{mutex};
    loggers.erase(std::remove(loggers.begin(), loggers.end(), logger), loggers.end());
}
