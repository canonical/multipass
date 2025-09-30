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

#include "powershell.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <shared/windows/process_factory.h>

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
    mpl::trace(name, fmt::format("PowerShell arguments: {}", power_shell->arguments().join(", ")));
    mpl::trace(name, fmt::format("PowerShell working dir '{}'", power_shell->working_directory()));
    mpl::trace(name, fmt::format("PowerShell program '{}'", power_shell->program()));

    // handle stdout and stderr separately
    power_shell->set_process_channel_mode(QProcess::SeparateChannels);

    QObject::connect(power_shell,
                     &mp::Process::state_changed,
                     [&name, power_shell](QProcess::ProcessState newState) {
                         mpl::trace(name,
                                    fmt::format("[{}] PowerShell state changed to {}",
                                                power_shell->process_id(),
                                                mpu::qenum_to_qstring(newState)));
                     });

    QObject::connect(power_shell,
                     &mp::Process::error_occurred,
                     [&name, power_shell](QProcess::ProcessError error) {
                         mpl::debug(name,
                                    fmt::format("[{}] PowerShell error occurred {}",
                                                power_shell->process_id(),
                                                mpu::qenum_to_qstring(error)));
                     });

    QObject::connect(power_shell,
                     &mp::Process::finished,
                     [&name, power_shell](mp::ProcessState state) {
                         const auto pid = power_shell->process_id();
                         if (state.completed_successfully())
                             mpl::debug(name,
                                        fmt::format("[{}] PowerShell finished successfully", pid));
                         else
                             mpl::warn(name,
                                       fmt::format("[{}] PowerShell finished abnormally: {}",
                                                   pid,
                                                   state.failure_message()));
                     });
}

const auto to_bare_csv_str =
    QStringLiteral("| ConvertTo-Csv -NoTypeInformation | Select-Object -Skip 1 "
                   "| foreach { $_ -replace '^\"|\"$|\"(?=,)|(?<=,)\"','' }"); /* this last bit
                   removes surrounding quotes; may be replaced with "-UseQuotes Never" in
                   powershell 7 */

} // namespace

const QStringList mp::PowerShell::Snippets::expand_property{"|",
                                                            "Select-Object",
                                                            "-ExpandProperty"};
const QStringList mp::PowerShell::Snippets::to_bare_csv =
    to_bare_csv_str.split(' ', Qt::SkipEmptyParts);

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
        auto msg =
            fmt::format("[{}] Failed to exit PowerShell gracefully", powershell_proc->process_id());
        if (!error.isEmpty())
            msg = fmt::format("{}: {}", msg, error);

        mpl::log_message(mpl::Level::warning, name, msg);
        powershell_proc->kill();
    }
}

void mp::PowerShell::easy_run(const QStringList& args, std::string&& error_msg)
{
    if (!run(args))
        throw std::runtime_error{std::move(error_msg)};
}

bool mp::PowerShell::run(const QStringList& args,
                         QString* output,
                         QString* output_err,
                         bool whisper)
{
    QString default_output, default_output_err;
    output = output ? output : &default_output;
    output_err = output_err ? output_err : &default_output_err;

    QString echo_cmdlet = QString("echo \"%1\" $?\n").arg(output_end_marker);
    bool cmdlet_code{false};
    auto pid = powershell_proc->process_id();
    auto notice_level = whisper ? mpl::Level::trace : mpl::Level::debug;
    const auto cmdlet = args.join(" ");

    mpl::log(notice_level, name, fmt::format("[{}] Cmdlet: '{}'", pid, cmdlet));

    // Have Powershell echo a unique string to differentiate between the cmdlet
    // output and the cmdlet exit status output
    if (write((cmdlet + "\n").toUtf8()) && write(echo_cmdlet.toUtf8()))
    {
        QString powershell_stdout;
        QString powershell_stderr;
        auto cmdlet_exit_found{false};
        while (!cmdlet_exit_found)
        {
            powershell_proc
                ->wait_for_ready_read(); // ignore timeouts - will just loop back if no output

            // Read stdout and stderr separately
            powershell_stdout.append(powershell_proc->read_all_standard_output());
            powershell_stderr.append(powershell_proc->read_all_standard_error());

            *output_err = powershell_stderr;

            if (powershell_stdout.contains(output_end_marker))
            {
                auto parsed_output = powershell_stdout.split(output_end_marker);
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

                    // Get the actual cmdlet's output
                    if (cmdlet_exit_found)
                    {
                        *output = parsed_output.at(0).trimmed();
                        mpl::log_message(mpl::Level::trace, name, output->toStdString());
                    }
                }
            }
        }

        // Always log stderr, even if command succeeded
        if (!powershell_stderr.isEmpty())
            mpl::warn(name, fmt::format("[{}] stderr: {}", pid, powershell_stderr));
    }

    // Log final output and status
    mpl::trace(name, fmt::format("[{}] Output: {}", pid, *output));
    mpl::log(notice_level, name, fmt::format("[{}] Cmdlet exit status is '{}'", pid, cmdlet_code));

    return cmdlet_code;
}

bool mp::PowerShell::exec(const QStringList& args,
                          const std::string& name,
                          QString* output,
                          QString* output_err)
{
    QString default_output{}, default_output_err{};
    output = output ? output : &default_output;
    output_err = output_err ? output_err : &default_output_err;

    auto power_shell = MP_PROCFACTORY.create_process(ps_cmd, args);
    setup_powershell(power_shell.get(), name);

    QObject::connect(
        power_shell.get(),
        &mp::Process::ready_read_standard_output,
        [output, &power_shell]() { *output += power_shell->read_all_standard_output(); });

    QObject::connect(
        power_shell.get(),
        &mp::Process::ready_read_standard_error,
        [&output_err, &power_shell]() { *output_err += power_shell->read_all_standard_error(); });

    power_shell->start();
    auto wait_result = power_shell->wait_for_finished(/* msecs = */ 60000);
    auto pid = power_shell->process_id(); // This is 0 iff the process didn't even start

    if (!wait_result)
    {
        if (pid)
            mpl::warn(name, "[{}] Process failed; {}", pid, power_shell->error_string());
        else
            mpl::warn(name, "Could not start PowerShell");
    }

    *output = output->trimmed();

    // Log stderr if any
    if (!output_err->isEmpty())
        mpl::warn(name, fmt::format("[{}] stderr: {}", pid, *output_err));

    mpl::trace(name, fmt::format("[{}] Output:\n{}", pid, *output));

    return wait_result && power_shell->process_state().completed_successfully();
}

bool mp::PowerShell::write(const QByteArray& data)
{
    if (auto written = powershell_proc->write(data); written < data.size())
    {
        auto msg = fmt::format("[{}] Failed to send input data '{}'.",
                               powershell_proc->process_id(),
                               data);
        if (written > 0)
            msg = fmt::format("{}. Only the first {} bytes were written", msg, written);

        mpl::log_message(mpl::Level::warning, name, msg);
        return false;
    }

    return true;
}
