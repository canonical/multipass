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

#include <multipass/process.h>

namespace mp = multipass;

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
