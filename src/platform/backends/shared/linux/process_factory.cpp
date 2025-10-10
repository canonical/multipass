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

#include "process_factory.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform_unix.h>
#include <multipass/process/basic_process.h>
#include <multipass/process/process_spec.h>
#include <multipass/process/simple_process_spec.h>
#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
class AppArmoredProcess : public mp::BasicProcess
{
public:
    AppArmoredProcess(const mp::AppArmor& aa, std::shared_ptr<mp::ProcessSpec> spec)
        : mp::BasicProcess{spec},
          apparmor{aa},
          aa_exec_str(fmt::format("exec {0}",
                                  process_spec->apparmor_profile_name().toLatin1().toStdString()))
    {
        apparmor.load_policy(process_spec->apparmor_profile().toLatin1());

        connect(this, &AppArmoredProcess::state_changed, [this](QProcess::ProcessState state) {
            if (state == QProcess::Starting)
            {
                mpl::log(mpl::Level::debug,
                         "daemon",
                         fmt::format("Applied AppArmor policy: {}",
                                     process_spec->apparmor_profile_name()));
            }
        });
    }

    void setup_child_process() final
    {
        // This function runs after fork, but before exec, which is a perfect
        // place to reset the signal masks.
        //
        // BEWARE: This function is called right in between fork and exec. Since this application is
        // multithreaded, we need to be *very* careful; fork replicates the state of mutexes and
        // other pthreads constructs, so we can only call async-signal-safe functions:
        //
        // https://man7.org/linux/man-pages/man7/signal-safety.7.html
        //
        // No heap allocations, no exceptions, basically treat this as a signal handler and adhere
        // by all the things you can't do in a signal handler. Further info:
        //
        // https://doc.qt.io/qt-6/qprocess.html#setChildProcessModifier
        //
        // Also, be careful about the stack allocations, too. The clone() call is being
        // made with CLONE_VM and the older QT versions has only 4096 bytes of reserved
        // stack space for the child:
        //
        // https://github.com/qt/qtbase/commit/993db5a12227b1e4067714ddc626d64a14474a54

        mp::BasicProcess::setup_child_process();

        // Unblock all signals for the child process
        {
            sigset_t sigset;
            sigemptyset(&sigset);
            sigprocmask(SIG_SETMASK, &sigset, nullptr);
        }

        // In the same spirit above, use our fork-friendly aa_change_onexec implementation
        mp::AppArmor::aa_change_onexec_forksafe(aa_exec_str.c_str());
    }

    ~AppArmoredProcess()
    {
        try
        {
            apparmor.remove_policy(process_spec->apparmor_profile().toLatin1());
        }
        catch (const std::exception& e)
        {
            // It's not considered an error when an apparmor cannot be removed
            mpl::log(mpl::Level::info, "apparmor", e.what());
        }
    }

private:
    const mp::AppArmor& apparmor;

    // This has to be pre-populated since we can't allocate memory in fork() state.
    const std::string aa_exec_str;
};

std::optional<mp::AppArmor> create_apparmor()
{
    try
    {
        mp::utils::snap_dir();
    }
    catch (const mp::SnapEnvironmentException&)
    {
        if (qEnvironmentVariableIsSet("DISABLE_APPARMOR"))
        {
            mpl::log(mpl::Level::warning, "apparmor", "AppArmor disabled by environment variable");
            return std::nullopt;
        }
    }

    try
    {
        mpl::log(mpl::Level::info, "apparmor", "Using AppArmor support");
        return mp::AppArmor{};
    }
    catch (mp::AppArmorException& e)
    {
        mpl::log(mpl::Level::warning,
                 "apparmor",
                 fmt::format("Failed to enable AppArmor: {}", e.what()));
        return std::nullopt;
    }
}
} // namespace

mp::ProcessFactory::ProcessFactory(const Singleton<ProcessFactory>::PrivatePass& pass)
    : Singleton<ProcessFactory>::Singleton{pass}, apparmor{create_apparmor()}
{
}

// This is the default ProcessFactory that creates a Process with no security mechanisms enabled
std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(
    std::unique_ptr<mp::ProcessSpec>&& process_spec) const
{
    if (apparmor && !process_spec->apparmor_profile().isNull())
    {
        std::shared_ptr<ProcessSpec> spec = std::move(process_spec);
        try
        {
            return std::make_unique<AppArmoredProcess>(apparmor.value(), spec);
        }
        catch (const mp::AppArmorException& e)
        {
            // TODO: This won't fly in strict mode (#1074), since we'll be confined by snapd
            mpl::log(mpl::Level::warning, "apparmor", e.what());
            return std::make_unique<BasicProcess>(spec);
        }
    }
    else
    {
        return std::make_unique<BasicProcess>(std::move(process_spec));
    }
}

std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(const QString& command,
                                                                const QStringList& arguments) const
{
    return create_process(simple_process_spec(command, arguments));
}
