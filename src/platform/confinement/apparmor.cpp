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

#include "apparmor.h"

#include <multipass/logging/log.h>
#include <sys/apparmor.h>

#include <QProcess>

#include <fmt/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
static const auto apparmor_parser = "apparmor_parser";

void throw_if_binary_fails(const char* binary_name, const QStringList& arguments = QStringList())
{
    QProcess process;
    process.start(binary_name, arguments);
    if (!process.waitForFinished() || process.exitCode() != 0)
    {
        throw std::runtime_error(
            fmt::format("AppArmor cannot be configured, the '{}' utility failed to launch with error: {}", binary_name,
                        qPrintable(process.errorString())));
    }
}

} // namespace

mp::AppArmor::AppArmor()
{
    int ret = aa_is_enabled();
    if (ret < 0)
    {
        throw std::runtime_error("AppArmor is not enabled");
    }

    // libapparmor's profile management API is not easy to use, it is handier to use apparmor_profile CLI tool
    // Ensure it is available
    throw_if_binary_fails(apparmor_parser, {"-V"});
}

void mp::AppArmor::load_policy(const QByteArray& aa_policy) const
{
    QProcess process;
    process.start(apparmor_parser, {"--abort-on-error", "-r"}); // inserts new or replaces existing
    process.waitForStarted();
    process.write(aa_policy);
    process.closeWriteChannel();
    process.waitForFinished();

    mpl::log(mpl::Level::debug, "daemon", fmt::format("Loading AppArmor policy: \n{}", aa_policy.constData()));

    if (process.exitCode() != 0)
    {
        throw std::runtime_error(fmt::format("Failed to load AppArmor policy {}: errno={} ({})", aa_policy.constData(),
                                             process.exitCode(), process.readAll().constData()));
    }
}

void mp::AppArmor::remove_policy(const QByteArray& aa_policy) const
{
    QProcess process;
    process.start(apparmor_parser, {"-R"});
    process.waitForStarted();
    process.write(aa_policy);
    process.closeWriteChannel();
    process.waitForFinished();

    mpl::log(mpl::Level::debug, "daemon", fmt::format("Removing AppArmor policy: \n{}", aa_policy.constData()));

    if (process.exitCode() != 0)
    {
        throw std::runtime_error(fmt::format("Failed to remove AppArmor policy {}: errno={} ({})",
                                             aa_policy.constData(), process.exitCode(), process.readAll().constData()));
    }
}

void mp::AppArmor::next_exec_under_policy(const QByteArray& aa_policy_name) const
{
    int ret = aa_change_onexec(aa_policy_name.constData());
    mpl::log(mpl::Level::debug, "daemon", fmt::format("Applying AppArmor policy: {}", aa_policy_name.constData()));

    if (ret < 0)
    {
        throw std::runtime_error(fmt::format("Failed to apply AppArmor policy {}: errno={} ({})",
                                             aa_policy_name.constData(), errno, strerror(errno)));
    }
}
