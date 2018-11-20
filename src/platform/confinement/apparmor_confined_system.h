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

#ifndef MULTIPASS_APPARMOR_CONFINED_SYSTEM_H
#define MULTIPASS_APPARMOR_CONFINED_SYSTEM_H

#include <multipass/confinement_system.h>

#include "apparmor.h"

namespace multipass
{

class AppArmorConfinedSystem : public ConfinementSystem
{
public:
    AppArmorConfinedSystem();
    virtual ~AppArmorConfinedSystem() = default;

    std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&& process_spec) const override;

private:
    const AppArmor apparmor;
};

} // namespace multipass

#endif // MULTIPASS_APPARMOR_CONFINED_SYSTEM_H
