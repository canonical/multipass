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

#include "process.h"
#include "process_spec.h"

#include <multipass/logging/log.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
std::string processErrorToString(QProcess::ProcessError error)
{
    switch (error)
    {
    case QProcess::FailedToStart:
        return "Process failed to start";
    case QProcess::Crashed:
        return "Process crashed";
    case QProcess::Timedout:
        return "waitFor..() timed out, process state unchanged";
    case QProcess::WriteError:
        return "Process write error";
    case QProcess::ReadError:
        return "Process read error";
    case QProcess::UnknownError:
        return "Unknown error occurred";
    default:
        return ""; // REMOVEME
    }
}
} // namespace

mp::Process::Process(std::unique_ptr<mp::ProcessSpec>&& spec) : process_spec{std::move(spec)}
{
    setProgram(process_spec->program());
    setArguments(process_spec->arguments());
    setProcessEnvironment(process_spec->environment());
    setProcessChannelMode(QProcess::ForwardedErrorChannel); // default to forwarding child stderr to console
}

void mp::Process::start(const QStringList& extra_arguments)
{
    QProcess::start(program(), arguments() << extra_arguments);
}

bool mp::Process::run_and_return_status(const QStringList& extra_arguments, const int timeout)
{
    start(extra_arguments);
    if (!waitForFinished(timeout))
    {
        mpl::log(mpl::Level::info, qPrintable(process_spec->program()), processErrorToString(error()));
        return false;
    }

    return exitStatus() == QProcess::NormalExit && exitCode() == 0;
}

QString mp::Process::run_and_return_output(const QStringList& extra_arguments, const int timeout)
{
    start(extra_arguments);
    if (!waitForFinished(timeout))
    {
        mpl::log(mpl::Level::info, qPrintable(process_spec->program()), processErrorToString(error()));
    }
    return readAllStandardOutput().trimmed();
}

void mp::Process::setupChildProcess()
{
    process_spec->setup_child_process();
}
