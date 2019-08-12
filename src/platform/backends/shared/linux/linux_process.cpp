/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "linux_process.h"
#include "process_spec.h"

#include <multipass/logging/log.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::LinuxProcess::LinuxProcess(std::unique_ptr<mp::ProcessSpec>&& spec) : process_spec{std::move(spec)}
{
    connect(&process, &QProcess::started, this, &mp::Process::started);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            [this](int exit_code, QProcess::ExitStatus exit_status) {
                mp::ProcessExitState exit_state;

                if (exit_status == QProcess::NormalExit)
                {
                    exit_state.exit_code = exit_code;
                }
                else // crash
                {
                    exit_state.error = mp::ProcessExitState::Error{process.error(), process.errorString()};
                }
                emit mp::Process::finished(exit_state);
            });
    connect(&process, &QProcess::errorOccurred, this, &mp::Process::error_occurred);
    connect(&process, &QProcess::readyReadStandardOutput, this, &mp::Process::ready_read_standard_output);
    connect(&process, &QProcess::readyReadStandardError, this, &mp::Process::ready_read_standard_error);

    process.setProgram(process_spec->program());
    process.setArguments(process_spec->arguments());
    process.setProcessEnvironment(process_spec->environment());
    if (!process_spec->working_directory().isNull())
        process.setWorkingDirectory(process_spec->working_directory());

    // TODO: multiline output produces poor formatting in logs, needs improving
    QObject::connect(&process, &QProcess::readyReadStandardError, [this]() {
        mpl::log(process_spec->error_log_level(), qPrintable(process_spec->program()),
                 qPrintable(process.readAllStandardError()));
    });
}

QString mp::LinuxProcess::program() const
{
    return process.program();
}

QStringList mp::LinuxProcess::arguments() const
{
    return process.arguments();
}

QString mp::LinuxProcess::working_directory() const
{
    return process.workingDirectory();
}

QProcessEnvironment mp::LinuxProcess::process_environment() const
{
    return process.processEnvironment();
}

void mp::LinuxProcess::start()
{
    process.start();
}

void mp::LinuxProcess::kill()
{
    process.kill();
}

mp::optional<mp::ProcessExitState> mp::LinuxProcess::wait_for_started(int msecs)
{
    // If process has already quit ok
    if (process.exitStatus() == QProcess::NormalExit)
    {
        mp::optional<mp::ProcessExitState> error_state;
        error_state->exit_code = process.exitCode();
        return error_state;
    }

    // Now try waiting for process, and if it fails get error condition.
    if (!process.waitForStarted(msecs))
    {
        mp::optional<mp::ProcessExitState> error_state;
        error_state->error = mp::ProcessExitState::Error{process.error(), process.errorString()};
        return error_state;
    }
    return mp::nullopt;
}

const mp::ProcessExitState mp::LinuxProcess::wait_for_finished(int msecs)
{
    mp::ProcessExitState error_state;

    // If process has already quit ok
    if (process.exitStatus() == QProcess::NormalExit)
    {
        error_state.exit_code = process.exitCode();
        return error_state;
    }

    // Now try waiting for process, and if it has failed get error condition.
    if (!process.waitForFinished(msecs))
    {
        error_state.error = mp::ProcessExitState::Error{process.error(), process.errorString()};
        return error_state;
    }
    return error_state;
}

bool mp::LinuxProcess::running() const
{
    return process.state() == QProcess::Running;
}

QByteArray mp::LinuxProcess::read_all_standard_output()
{
    return process.readAllStandardOutput();
}

QByteArray mp::LinuxProcess::read_all_standard_error()
{
    return process.readAllStandardError();
}

qint64 mp::LinuxProcess::write(const QByteArray& data)
{
    return process.write(data);
}

void mp::LinuxProcess::close_write_channel()
{
    process.closeWriteChannel();
}

const mp::ProcessExitState mp::LinuxProcess::execute(const int timeout)
{
    mp::ProcessExitState exit_state;
    start();

    if (!process.waitForFinished(timeout) || process.exitStatus() != QProcess::NormalExit)
    {
        mpl::log(mpl::Level::error, qPrintable(process_spec->program()), qPrintable(process.errorString()));
        exit_state.error = mp::ProcessExitState::Error{process.error(), process.errorString()};
        return exit_state;
    }

    exit_state.exit_code = process.exitCode();
    return exit_state;
}
