/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "powershell.h"

#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <QMetaEnum>
#include <QProcess>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const QString unique_echo_string{"cmdlet status is"};

void setup_powershell(QProcess* power_shell, const QStringList& args, const std::string& name)
{
    power_shell->setProgram("powershell.exe");

    power_shell->setArguments(args);
    power_shell->setProcessChannelMode(QProcess::MergedChannels);

    mpl::log(mpl::Level::debug, name,
             fmt::format("powershell arguments '{}'", power_shell->arguments().join(", ").toStdString()));

    mpl::log(mpl::Level::debug, name,
             fmt::format("powershell working dir '{}'", power_shell->workingDirectory().toStdString()));
    mpl::log(mpl::Level::debug, name, fmt::format("powershell program '{}'", power_shell->program().toStdString()));

    QObject::connect(power_shell, &QProcess::started,
                     [&name]() { mpl::log(mpl::Level::debug, name, "powershell started"); });

    QObject::connect(power_shell, &QProcess::stateChanged, [&name](QProcess::ProcessState newState) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessState>();
        mpl::log(mpl::Level::debug, name, fmt::format("powershell state changed to {}", meta.valueToKey(newState)));
    });

    QObject::connect(power_shell, &QProcess::errorOccurred, [&name](QProcess::ProcessError error) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessError>();
        mpl::log(mpl::Level::debug, name, fmt::format("powershell error occurred {}", meta.valueToKey(error)));
    });

    QObject::connect(power_shell, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     [&name](int exitCode, QProcess::ExitStatus exitStatus) {
                         mpl::log(mpl::Level::debug, name,
                                  fmt::format("powershell finished with exit code {}", exitCode));
                     });
}
} // namespace

mp::PowerShell::PowerShell(const std::string& name) : name(name)
{
    setup_powershell(&powershell_proc, {"-NoExit", "-Command", "-"}, this->name);

    powershell_proc.start();
}

mp::PowerShell::~PowerShell()
{
    powershell_proc.write("Exit\n");
    powershell_proc.waitForFinished();
}

bool mp::PowerShell::run(const QStringList& args, QString& output)
{
    QString echo_cmdlet = QString("echo \"%1\" $?\n").arg(unique_echo_string);
    bool cmdlet_code{false};

    mpl::log(mpl::Level::trace, name,
             fmt::format("cmdlet: '{}'", args.join(" ").toStdString()));
    powershell_proc.write(args.join(" ").toUtf8() + "\n");

    // Have Powershell echo a unique string to differentiate between the cmdlet
    // output and the cmdlet exit status output
    powershell_proc.write(echo_cmdlet.toUtf8());

    QString powershell_output;
    auto cmdlet_exit_found{false};
    while (!cmdlet_exit_found)
    {
        powershell_proc.waitForReadyRead();

        powershell_output.append(powershell_proc.readAllStandardOutput());
        if (powershell_output.contains(unique_echo_string))
        {
            auto parsed_output = powershell_output.split(unique_echo_string);
            if (parsed_output.size() == 2)
            {
                // Be sure the exit status is fully read from output
                // Exit status can only be "True" or "False"
                auto exit_value = parsed_output.at(1);
                if (exit_value.contains("True"))
                {
                    cmdlet_code = true;
                    cmdlet_exit_found = true;
                }
                else if (exit_value.contains("False"))
                {
                    cmdlet_code = false;
                    cmdlet_exit_found = true;
                }
            }

            // Get the actual cmdlet's output
            if (cmdlet_exit_found)
            {
                output = parsed_output.at(0).trimmed();
                mpl::log(mpl::Level::trace, name, output.toStdString());
            }
        }
    }

    mpl::log(mpl::Level::trace, name, fmt::format("cmdlet exit status is '{}'", cmdlet_code));
    return cmdlet_code;
}

bool mp::PowerShell::exec(const QStringList& args, const std::string& name, QString& output)
{
    QProcess power_shell;
    setup_powershell(&power_shell, args, name);

    QObject::connect(&power_shell, &QProcess::readyReadStandardOutput, [&name, &output, &power_shell]() {
        output += power_shell.readAllStandardOutput().trimmed();
        mpl::log(mpl::Level::trace, name, output.toStdString());
    });

    power_shell.start();
    power_shell.waitForFinished();

    return power_shell.exitStatus() == QProcess::NormalExit && power_shell.exitCode() == 0;
}
