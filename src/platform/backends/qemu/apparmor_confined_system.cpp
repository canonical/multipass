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
    AppArmoredProcess(const mp::AppArmor& apparmor,
                                         std::unique_ptr<mp::ProcessSpec>&& process_spec)
        : mp::Process(std::move(process_spec)), apparmor{apparmor}
    {
    }

    ~AppArmoredProcess()
    {
        // TODO: remove registered AA profile
    }

    void start()
    {
        apparmor.load_policy(process_spec->apparmor_profile());

        // GERRY: what do I do if not running under snap?? Or AppArmor not available??
        // process.start("aa-exec", QStringList() << "-p" << apparmor_profile_name() << "--" << program() <<
        // arguments());
        // alternative is to hook into the QPRocess::stateChanged -> Starting signal (blocked), and call the apparmor
        // aa_change_onexec which hopefully (pending race conditions) will attach to the correct exec call.
        mp::Process::start();
    }

private:
    const mp::AppArmor& apparmor;
};

} // namespace



mp::AppArmorConfinedSystem::AppArmorConfinedSystem()
{}

std::unique_ptr<mp::Process> mp::AppArmorConfinedSystem::create_process(std::unique_ptr<ProcessSpec> &&process_spec) const
{
    return std::make_unique<::AppArmoredProcess>(apparmor, std::move(process_spec));
}
