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

#include "apparmored_process.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/logging/log.h>
#include <multipass/process/basic_process.h>
#include <multipass/process/process_spec.h>
#include <multipass/process/simple_process_spec.h>
#include <multipass/snap_utils.h>

#include <QFileInfo>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
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
            mpl::warn("apparmor", "AppArmor disabled by environment variable");
            return std::nullopt;
        }
    }

    try
    {
        mpl::info("apparmor", "Using AppArmor support");
        return mp::AppArmor{};
    }
    catch (mp::AppArmorException& e)
    {
        mpl::warn("apparmor", "Failed to enable AppArmor: {}", e.what());
        return std::nullopt;
    }
}

QString make_apparmor_profile_name(const mp::ProcessSpec& process_spec, uint64_t process_num)
{
    const QString identifier = process_spec.identifier();
    const QString exe_path = QFileInfo{process_spec.program()}.fileName();

    if (identifier.isNull())
        return QString{"multipass.%1.%2"}.arg(process_num).arg(exe_path);
    else
        return QString{"multipass.%1.%2.%3"}.arg(process_num).arg(identifier).arg(exe_path);
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
    const uint64_t process_num = process_counter.fetch_add(1);

    const auto profile_name = make_apparmor_profile_name(*process_spec, process_num);
    const auto policy = process_spec->apparmor_profile(profile_name).toLatin1();

    if (apparmor && !policy.isNull())
    {
        std::shared_ptr<ProcessSpec> spec = std::move(process_spec);
        try
        {
            return std::make_unique<mp::AppArmoredProcess>(apparmor.value(),
                                                           spec,
                                                           profile_name,
                                                           policy);
        }
        catch (const mp::AppArmorException& e)
        {
            // TODO: This won't fly in strict mode (#1074), since we'll be confined by snapd
            mpl::log_message(mpl::Level::warning, "apparmor", e.what());
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
