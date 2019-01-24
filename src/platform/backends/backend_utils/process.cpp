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

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/prctl.h>

namespace mp = multipass;

mp::Process::Process(std::unique_ptr<mp::ProcessSpec> &&spec)
    : process_spec{std::move(spec)}
    , parent_pid(getpid())
{
    setProgram(process_spec->program());
    setArguments(process_spec->arguments());
    setProcessEnvironment(process_spec->environment());
}

void mp::Process::start(const QStringList& extra_arguments)
{
    QProcess::start(program(), arguments() << extra_arguments);
}

bool mp::Process::run_and_return_status(const QStringList& extra_arguments, const int timeout)
{
    start(extra_arguments);
    waitForFinished(timeout);

    return exitStatus() == QProcess::NormalExit && exitCode() == 0;
}

QString mp::Process::run_and_return_output(const QStringList& extra_arguments, const int timeout)
{
    start(extra_arguments);
    waitForFinished(timeout);

    return readAllStandardOutput().trimmed();
}

void mp::Process::setupChildProcess()
{
    // Send sigterm to child when its parent process dies unexpectedly
    int r = prctl(PR_SET_PDEATHSIG, process_spec->stop_signal());
    if (r == -1)
    {
        perror(0);
        exit(1);
    }
    // test in case the original parent exited just before the prctl() call
    if (getppid() != parent_pid)
        exit(1);
}
