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

#include "syslog_logger.h"
#include <syslog.h>

namespace mpl = multipass::logging;

mpl::SyslogLogger::SyslogLogger(mpl::Level level) : LinuxLogger{level}
{
    openlog("multipass", LOG_CONS | LOG_PID, LOG_USER);
}

void mpl::SyslogLogger::log(mpl::Level level, CString category, CString message) const
{
    if (level <= logging_level)
    {
        syslog(to_syslog_priority(level), "[%s] %s", category.c_str(), message.c_str());
    }
}
