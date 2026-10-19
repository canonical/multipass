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

#include "journald_logger.h"
#include "journald_wrapper.h"

namespace multipass::logging
{

JournaldLogger::JournaldLogger(Level level) : LinuxLogger{level}
{
}

void JournaldLogger::log(Level level, std::string_view category, std::string_view message) const
{
    if (level <= logging_level)
    {
        constexpr static std::string_view message_fmt_str = "MESSAGE=%.*s";
        constexpr static std::string_view priority_fmt_str = "PRIORITY=%i";
        constexpr static std::string_view category_fmt_str = "CATEGORY=%.*s";

        JournaldWrapper::instance().write_journal(message_fmt_str,
                                                  message,
                                                  priority_fmt_str,
                                                  to_syslog_priority(level),
                                                  category_fmt_str,
                                                  category);
    }
}

} // namespace multipass::logging
