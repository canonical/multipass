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

#include <multipass/confinement_system.h>

#include "apparmor_confined_system.h"
#include "unconfined_system.h"

namespace mp = multipass;

std::shared_ptr<mp::ConfinementSystem> mp::ConfinementSystem::create_confinement_system()
{
    const auto disable_apparmor = qgetenv("DISABLE_APPARMOR");
    if (!disable_apparmor.isNull())
    {
        return std::make_shared<mp::UnconfinedSystem>();
    }

    return std::make_shared<mp::AppArmorConfinedSystem>();
}
