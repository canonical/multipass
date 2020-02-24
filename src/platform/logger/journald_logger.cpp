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

#include "journald_logger.h"

#define SD_JOURNAL_SUPPRESS_LOCATION
#include <syslog.h>
#include <systemd/sd-journal.h>

namespace mpl = multipass::logging;

namespace
{
constexpr auto to_syslog_priority(const mpl::Level& level) noexcept
{
    switch (level)
    {
    case mpl::Level::trace:
        return LOG_DEBUG;
    case mpl::Level::debug:
        return LOG_DEBUG;
    case mpl::Level::error:
        return LOG_ERR;
    case mpl::Level::info:
        return LOG_INFO;
    case mpl::Level::warning:
        return LOG_WARNING;
    }
    return 42;
}
} // namespace

mpl::JournaldLogger::JournaldLogger(mpl::Level level) : logging_level{level}
{
}

void mpl::JournaldLogger::log(mpl::Level level, CString category, CString message) const
{
    if (level <= logging_level)
    {
        sd_journal_send("MESSAGE=%s", message.c_str(), "PRIORITY=%i", to_syslog_priority(level), "CATEGORY=%s",
                        category.c_str(), nullptr);
    }
}
