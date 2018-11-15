/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "powershell.h"

#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <QMetaEnum>
#include <QProcess>

namespace mp = multipass;
namespace mpl = multipass::logging;

bool mp::powershell_run(const QStringList& args, const std::string& name, std::string& output)
{
    QProcess power_shell;
    power_shell.setProgram("powershell.exe");
    power_shell.setArguments(args);

    mpl::log(mpl::Level::debug, name,
             fmt::format("powershell working dir '{}'", power_shell.workingDirectory().toStdString()));
    mpl::log(mpl::Level::debug, name, fmt::format("powershell program '{}'", power_shell.program().toStdString()));
    mpl::log(mpl::Level::debug, name,
             fmt::format("powershell arguments '{}'", power_shell.arguments().join(", ").toStdString()));

    QObject::connect(&power_shell, &QProcess::started,
                     [&name]() { mpl::log(mpl::Level::debug, name, "powershell started"); });
    QObject::connect(&power_shell, &QProcess::readyReadStandardOutput, [&name, &output, &power_shell]() {
        output = power_shell.readAllStandardOutput().trimmed().toStdString();
        mpl::log(mpl::Level::debug, name, output);
    });

    QObject::connect(&power_shell, &QProcess::readyReadStandardError, [&name, &power_shell]() {
        mpl::log(mpl::Level::warning, name, power_shell.readAllStandardError().data());
    });

    QObject::connect(&power_shell, &QProcess::stateChanged, [&name](QProcess::ProcessState newState) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessState>();
        mpl::log(mpl::Level::debug, name, fmt::format("powershell state changed to {}", meta.valueToKey(newState)));
    });

    QObject::connect(&power_shell, &QProcess::errorOccurred, [&name](QProcess::ProcessError error) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessError>();
        mpl::log(mpl::Level::debug, name, fmt::format("powershell error occurred {}", meta.valueToKey(error)));
    });

    QObject::connect(&power_shell, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     [&name](int exitCode, QProcess::ExitStatus exitStatus) {
                         mpl::log(mpl::Level::debug, name,
                                  fmt::format("powershell finished with exit code {}", exitCode));
                     });

    power_shell.start();
    power_shell.waitForFinished();

    return power_shell.exitStatus() == QProcess::NormalExit && power_shell.exitCode() == 0;
}
