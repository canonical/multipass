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

#ifndef MULTIPASS_AUTOSTART_SETUP_EXCEPTION_H
#define MULTIPASS_AUTOSTART_SETUP_EXCEPTION_H

#include <stdexcept>
#include <string>

namespace multipass
{
class AutostartSetupException : public std::runtime_error
{
public:
    AutostartSetupException(const std::string& why, const std::string& detail) : runtime_error(why), detail{detail}
    {
    }

    const std::string& get_detail() const
    {
        return detail;
    }

private:
    std::string detail;
};
} // namespace multipass

#endif // MULTIPASS_AUTOSTART_SETUP_EXCEPTION_H
