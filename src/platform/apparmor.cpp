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

static const auto apparmor_parser = "apparmor_parser";
static const auto apparmor_exec = "aa-exec";

namespace
{

void throw_if_binary_fails(const char* binary_name, const QStringList& arguments = QStringList())
{
    QProcess process;
    process.start(binary_name, arguments);
    process.waitForFinished();
    if (process.exitCode() != 0)
    {
        throw std::runtime_error(fmt::format("AppArmor cannot be configured, '{}' failed to launch with error: {}",
                                             binary_name, qPrintable(process.errorString())));
    }
}

} // namespace

mp::AppArmor::AppArmor() : aa_interface(nullptr)
{
    int ret = aa_is_enabled();
    if (ret < 0)
    {
        throw std::runtime_error("AppArmor is not enabled");
    }

    // libapparmor's profile management APIis not easy to use, handier to use apparmor_profile CLI tool
    // and aa-exec to spawn child processes. Ensure they are available
    throw_if_binary_fails(apparmor_parser, {"-V"});
    throw_if_binary_fails(apparmor_exec);
}

mp::AppArmor::~AppArmor()
{
}

void mp::AppArmor::load_policy(const QByteArray& aa_policy) const
{
    QProcess process;
    process.start(apparmor_parser, {"--abort-on-error", "-r"}); // inserts new or replaces existing
    process.waitForStarted();
    process.write(aa_policy);
    process.closeWriteChannel();
    process.waitForFinished();

    if (process.exitCode() != 0)
    { // something went wrong
        throw std::runtime_error(fmt::format("Failed to load AppArmor policy: errno={} ({})", process.exitCode(),
                                             process.readAll().constData()));
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

    if (process.exitCode() != 0)
    { // something went wrong
        throw std::runtime_error(fmt::format("Failed to remove AppArmor policy: errno={} ({})", process.exitCode(),
                                             process.readAll().constData()));
    }
}
