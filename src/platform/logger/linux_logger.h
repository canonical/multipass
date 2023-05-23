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

#ifndef MULTIPASS_LINUX_LOGGER_H
#define MULTIPASS_LINUX_LOGGER_H

#include <multipass/logging/logger.h>
#include <syslog.h>

namespace multipass
{
namespace logging
{
class LinuxLogger : public Logger
{
public:
    explicit LinuxLogger(Level level);

protected:
    static constexpr auto to_syslog_priority(const Level& level) noexcept
    {
        switch (level)
        {
        case Level::trace:
        case Level::debug:
            return LOG_DEBUG;
        case Level::error:
            return LOG_ERR;
        case Level::info:
            return LOG_INFO;
        case Level::warning:
            return LOG_WARNING;
        }
        return 42;
    }
};
} // namespace logging
} // namespace multipass
#endif // MULTIPASS_LINUX_LOGGER_H
