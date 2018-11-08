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

#include "apparmor_confined_system.h"
#include "process.h"
#include "process_spec.h"

namespace mp = multipass;

namespace
{

class AppArmoredProcess : public mp::Process
{
public:
    AppArmoredProcess(const mp::AppArmor& aa, std::unique_ptr<mp::ProcessSpec>&& spec)
        : mp::Process(std::move(spec)), apparmor{aa}
    {
        apparmor.load_policy(process_spec->apparmor_profile().toLatin1());

        // This is the closest I can get to the exec() call in QProcess::start. Might be racey
        QObject::connect(this, &Process::stateChanged, [this](QProcess::ProcessState state) {
            if (state == QProcess::Starting)
            {
                apparmor.apply_policy_to_next_exec(process_spec->apparmor_profile_name().toLatin1());
            }
        });
    }

    ~AppArmoredProcess()
    {
        apparmor.remove_policy(process_spec->apparmor_profile().toLatin1());
    }

private:
    const mp::AppArmor& apparmor;
};

} // namespace

mp::AppArmorConfinedSystem::AppArmorConfinedSystem()
{
}

std::unique_ptr<mp::Process>
mp::AppArmorConfinedSystem::create_process(std::unique_ptr<ProcessSpec>&& process_spec) const
{
    return std::make_unique<::AppArmoredProcess>(apparmor, std::move(process_spec));
}
