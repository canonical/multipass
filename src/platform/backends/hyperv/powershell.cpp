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

void setup_powershell(mp::Process* power_shell, const std::string& name)
{
    mpl::log(mpl::Level::debug, name, fmt::format("PowerShell arguments '{}'", power_shell->arguments().join(", ")));
    mpl::log(mpl::Level::debug, name, fmt::format("PowerShell working dir '{}'", power_shell->working_directory()));
    mpl::log(mpl::Level::debug, name, fmt::format("PowerShell program '{}'", power_shell->program()));

    power_shell->set_process_channel_mode(QProcess::MergedChannels);

    QObject::connect(power_shell, &mp::Process::started,
                     [&name]() { mpl::log(mpl::Level::debug, name, "PowerShell started"); });

    QObject::connect(power_shell, &mp::Process::state_changed, [&name](QProcess::ProcessState newState) {
        mpl::log(mpl::Level::debug, name,
                 fmt::format("PowerShell state changed to {}", mpu::qenum_to_qstring(newState)));
    });

    QObject::connect(power_shell, &mp::Process::error_occurred, [&name](QProcess::ProcessError error) {
        mpl::log(mpl::Level::debug, name, fmt::format("PowerShell error occurred {}", mpu::qenum_to_qstring(error)));
    });

    QObject::connect(power_shell, &mp::Process::finished, [&name](mp::ProcessState state) {
        if (state.completed_successfully())
            mpl::log(mpl::Level::debug, name, "PowerShell finished successfully");
        else
            mpl::log(mpl::Level::warning, name,
                     fmt::format("PowerShell finished abnormally: {}", state.failure_message()));
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
    if (!write("Exit\n") || !powershell_proc->wait_for_finished())
    {
        auto error = powershell_proc->error_string();
        auto msg = std::string{"Failed to exit PowerShell gracefully"};
        if (!error.isEmpty())
            msg = fmt::format("{}: {}", msg, error);

        mpl::log(mpl::Level::warning, name, msg);
        powershell_proc->kill();
    }
}

bool mp::PowerShell::run(const QStringList& args, QString& output)
{
    QString echo_cmdlet = QString("echo \"%1\" $?\n").arg(output_end_marker);
    bool cmdlet_code{false};

    mpl::log(mpl::Level::trace, name, fmt::format("Cmdlet: '{}'", args.join(" ")));

    // Have Powershell echo a unique string to differentiate between the cmdlet
    // output and the cmdlet exit status output
    if (write(args.join(" ").toUtf8() + "\n") && write(echo_cmdlet.toUtf8()))
    {
        QString powershell_output;
        auto cmdlet_exit_found{false};
        while (!cmdlet_exit_found)
        {
            powershell_proc->wait_for_ready_read(); // ignore timeouts - will just loop back if no output

            powershell_output.append(powershell_proc->read_all_standard_output());
            if (powershell_output.contains(output_end_marker))
            {
                auto parsed_output = powershell_output.split(output_end_marker);
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
    }

    mpl::log(mpl::Level::trace, name, fmt::format("Cmdlet exit status is '{}'", cmdlet_code));
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
                 fmt::format("Cmdlet failed with {}: {}", power_shell->error_string(), args.join(" ")));

    output = output.trimmed();
    mpl::log(mpl::Level::trace, name, output.toStdString());

    return wait_result && power_shell->process_state().completed_successfully();
}

bool mp::PowerShell::write(const QByteArray& data)
{
    if (auto written = powershell_proc->write(data); written < data.size())
    {
        auto msg = fmt::format("Failed to send input data '{}'.", data);
        if (written > 0)
            msg = fmt::format("{}. Only the first {} bytes were written", msg, written);

        mpl::log(mpl::Level::warning, name, msg);
        return false;
    }

    return true;
}
