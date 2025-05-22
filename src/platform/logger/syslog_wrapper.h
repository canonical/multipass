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

#pragma once

#include <multipass/singleton.h>
#include <string_view>

namespace multipass
{
namespace logging
{
class SyslogWrapper : public Singleton<SyslogWrapper>
{
public:
    SyslogWrapper(const Singleton<SyslogWrapper>::PrivatePass&) noexcept;

    /**
     * Write a entry to the syslog
     *
     * @param [in] level Syslog log level
     * @param [in] format_string Format string. Must be NUL terminated.
     * @param [in] category Category of the message
     * @param [in] message The message
     *
     * NOTE: All format strings MUST be NUL-terminated.
     */
    virtual void write_syslog(int level,
                              std::string_view format_string,
                              std::string_view category,
                              std::string_view message) const;
};
} // namespace logging
} // namespace multipass
