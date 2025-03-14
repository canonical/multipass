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

#include "syslog_wrapper.h"

#include <syslog.h>

namespace multipass::logging
{

SyslogWrapper::SyslogWrapper(const Singleton<SyslogWrapper>::PrivatePass& pass) noexcept
    : Singleton<SyslogWrapper>::Singleton{pass}
{
    openlog("multipass", LOG_CONS | LOG_PID, LOG_USER);
}

void SyslogWrapper::write_syslog(int level,
                                 std::string_view format_string,
                                 std::string_view category,
                                 std::string_view message) const
{
    syslog(level,
           format_string.data(),
           static_cast<int>(category.size()),
           category.data(),
           static_cast<int>(message.size()),
           message.data());
}

} // namespace multipass::logging
