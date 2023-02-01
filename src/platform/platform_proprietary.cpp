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

#include <multipass/format.h>
#include <multipass/platform.h>

#include "platform_proprietary.h"

namespace mp = multipass;

QString mp::platform::Platform::get_blueprints_url_override() const
{

    if (check_unlock_code())
    {
        return QString::fromUtf8(qgetenv("MULTIPASS_BLUEPRINTS_URL"));
    }

    return {};
}

std::string mp::platform::host_version()
{
    return fmt::format("{}-{}", QSysInfo::productType(), QSysInfo::productVersion());
}
