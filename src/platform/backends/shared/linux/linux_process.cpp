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
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &mp::Process::finished);
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
    return process.arguments(); // as start() can add more, this most reliable source
}

QString mp::LinuxProcess::working_directory() const
{
    return process.workingDirectory();
}

QProcessEnvironment mp::LinuxProcess::process_environment() const
{
    return process.processEnvironment();
}

void mp::LinuxProcess::start(const QStringList& extra_arguments)
{
    process.setArguments(process_spec->arguments() << extra_arguments);
    process.start();
}

void mp::LinuxProcess::kill()
{
    process.kill();
}

bool mp::LinuxProcess::wait_for_started(int msecs)
{
    return process.waitForStarted(msecs);
}

bool mp::LinuxProcess::wait_for_finished(int msecs)
{
    return process.waitForFinished(msecs);
}

bool mp::LinuxProcess::running() const
{
    return process.state() == QProcess::Running;
}

QByteArray multipass::LinuxProcess::read_all_standard_output()
{
    return process.readAllStandardOutput();
}

QByteArray multipass::LinuxProcess::read_all_standard_error()
{
    return process.readAllStandardError();
}

qint64 mp::LinuxProcess::write(const QByteArray& data)
{
    return process.write(data);
}

bool mp::LinuxProcess::run_and_return_status(const QStringList& extra_arguments, const int timeout)
{
    run_and_wait_until_finished(extra_arguments, timeout);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString mp::LinuxProcess::run_and_return_output(const QStringList& extra_arguments, const int timeout)
{
    run_and_wait_until_finished(extra_arguments, timeout);
    return process.readAllStandardOutput().trimmed();
}

void mp::LinuxProcess::run_and_wait_until_finished(const QStringList& extra_arguments, const int timeout)
{
    start(extra_arguments);
    if (!process.waitForFinished(timeout) || process.exitStatus() != QProcess::NormalExit)
    {
        mpl::log(mpl::Level::error, qPrintable(process_spec->program()), qPrintable(process.errorString()));
    }
}
