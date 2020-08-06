/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <shared/win/process_factory.h>

#include <QProcess>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto ps_cmd = "powershell.exe";
const auto default_args = QStringList{"-NoProfile", "-NoExit", "-Command", "-"};
const QString unique_echo_string{"cmdlet status is"};

void setup_powershell(mp::Process* power_shell, const std::string& name)
{
    power_shell->set_process_channel_mode(QProcess::MergedChannels);

    mpl::log(mpl::Level::debug, name, fmt::format("powershell arguments '{}'", power_shell->arguments().join(", ")));

    mpl::log(mpl::Level::debug, name, fmt::format("powershell working dir '{}'", power_shell->working_directory()));
    mpl::log(mpl::Level::debug, name, fmt::format("powershell program '{}'", power_shell->program()));
    QObject::connect(power_shell, &mp::Process::started,
                     [&name]() { mpl::log(mpl::Level::debug, name, "powershell started"); });

    QObject::connect(power_shell, &mp::Process::state_changed, [&name](QProcess::ProcessState newState) {
        mpl::log(mpl::Level::debug, name,
                 fmt::format("powershell state changed to {}", mpu::qenum_to_qstring(newState)));
    });

    QObject::connect(power_shell, &mp::Process::error_occurred, [&name](QProcess::ProcessError error) {
        mpl::log(mpl::Level::debug, name, fmt::format("powershell error occurred {}", mpu::qenum_to_qstring(error)));
    });

    QObject::connect(power_shell, &mp::Process::finished, [&name](mp::ProcessState state) {
        if (state.completed_successfully())
            mpl::log(mpl::Level::debug, name, "powershell finished successfully");
        else
            mpl::log(mpl::Level::warning, name,
                     fmt::format("powershell finished abnormally: {}", state.failure_message()));
    });
}

} // namespace

mp::PowerShell::PowerShell(const std::string& name)
    : powershell_proc{MP_PROCFACTORY.create_process(ps_cmd, default_args)}, name(name)
{
    setup_powershell(powershell_proc.get(), this->name);

    powershell_proc->start();
}

mp::PowerShell::~PowerShell()
{
    powershell_proc->write("Exit\n");     // TODO@ricab check return
    powershell_proc->wait_for_finished(); // TODO@ricab check return
}

bool mp::PowerShell::run(const QStringList& args, QString& output)
{
    QString echo_cmdlet = QString("echo \"%1\" $?\n").arg(unique_echo_string);
    bool cmdlet_code{false};

    mpl::log(mpl::Level::trace, name, fmt::format("cmdlet: '{}'", args.join(" ")));
    powershell_proc->write(args.join(" ").toUtf8() + "\n"); // TODO@ricab check return

    // Have Powershell echo a unique string to differentiate between the cmdlet
    // output and the cmdlet exit status output
    powershell_proc->write(echo_cmdlet.toUtf8()); // TODO@ricab check return

    QString powershell_output;
    auto cmdlet_exit_found{false};
    while (!cmdlet_exit_found)
    {
        powershell_proc->wait_for_ready_read(); // ignore timeouts - will just loop back if no output

        powershell_output.append(powershell_proc->read_all_standard_output());
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
    auto power_shell = MP_PROCFACTORY.create_process(ps_cmd, args);
    setup_powershell(power_shell.get(), name);

    QObject::connect(power_shell.get(), &mp::Process::ready_read_standard_output,
                     [&output, &power_shell]() { output += power_shell->read_all_standard_output(); });

    power_shell->start();
    auto wait_result = power_shell->wait_for_finished();
    if (!wait_result)
        mpl::log(mpl::Level::warning, name,
                 fmt::format("cmdlet failed with {}: {}", power_shell->error_string(), args.join(" ")));

    output = output.trimmed();
    mpl::log(mpl::Level::trace, name, output.toStdString());

    return wait_result && power_shell->process_state().completed_successfully();
}
