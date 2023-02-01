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
        : mp::BasicProcess{spec}, apparmor{aa}
    {
        apparmor.load_policy(process_spec->apparmor_profile().toLatin1());

        connect(this, &AppArmoredProcess::state_changed, [this](QProcess::ProcessState state) {
            if (state == QProcess::Starting)
            {
                mpl::log(mpl::Level::debug, "daemon",
                         fmt::format("Applied AppArmor policy: {}", process_spec->apparmor_profile_name()));
            }
        });
    }

    void setup_child_process() final
    {
        mp::BasicProcess::setup_child_process();

        apparmor.next_exec_under_policy(process_spec->apparmor_profile_name().toLatin1());
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
        mpl::log(mpl::Level::warning, "apparmor", fmt::format("Failed to enable AppArmor: {}", e.what()));
        return std::nullopt;
    }
}
} // namespace

mp::ProcessFactory::ProcessFactory(const Singleton<ProcessFactory>::PrivatePass& pass)
    : Singleton<ProcessFactory>::Singleton{pass}, apparmor{create_apparmor()}
{
}

// This is the default ProcessFactory that creates a Process with no security mechanisms enabled
std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(std::unique_ptr<mp::ProcessSpec>&& process_spec) const
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
