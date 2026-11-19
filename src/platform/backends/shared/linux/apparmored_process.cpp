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
 */

#include "apparmored_process.h"

#include "apparmor.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform_unix.h>

#include <exception>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::AppArmoredProcess::AppArmoredProcess(const AppArmor& apparmor,
                                         std::shared_ptr<ProcessSpec> process_spec,
                                         QString profile_name,
                                         QByteArray policy)
    : BasicProcess{std::move(process_spec)},
      apparmor{apparmor},
      profile_name{std::move(profile_name)},
      policy{std::move(policy)},
      aa_exec_str{fmt::format("exec {0}", this->profile_name.toLatin1().toStdString())}
{
    apparmor.load_policy(this->policy);

    connect(this, &AppArmoredProcess::state_changed, [this](QProcess::ProcessState state) {
        if (state == QProcess::Starting)
            mpl::debug("daemon", "Applied AppArmor policy: {}", this->profile_name);
    });
}

mp::AppArmoredProcess::~AppArmoredProcess()
{
    try
    {
        apparmor.remove_policy(policy);
    }
    catch (const std::exception& e)
    {
        // It's not considered an error when an AppArmor policy cannot be removed.
        mpl::log_message(mpl::Level::info, "apparmor", e.what());
    }
}

const QString& mp::AppArmoredProcess::apparmor_profile_name() const noexcept
{
    return profile_name;
}

const QByteArray& mp::AppArmoredProcess::apparmor_profile() const noexcept
{
    return policy;
}

void mp::AppArmoredProcess::setup_child_process()
{
    // This function runs after fork, but before exec. Since this application is multithreaded,
    // only async-signal-safe operations are allowed here. Also avoid large stack allocations:
    // older Qt versions reserve only 4096 bytes for the child when using CLONE_VM.
    mp::BasicProcess::setup_child_process();

    sigset_t sigset;
    sigemptyset(&sigset);
    sigprocmask(SIG_SETMASK, &sigset, nullptr);

    // Use the fork-safe implementation because libapparmor's aa_change_onexec allocates memory.
    mp::AppArmor::aa_change_onexec_forksafe(aa_exec_str.c_str());
}
