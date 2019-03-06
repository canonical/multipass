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

#include "process.h"
#include "process_spec.h"

#include <multipass/logging/log.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::Process::Process(std::unique_ptr<mp::ProcessSpec>&& spec) : process_spec{std::move(spec)}
{
    setProgram(process_spec->program());
    setArguments(process_spec->arguments());
    setProcessEnvironment(process_spec->environment());
    if (!process_spec->working_directory().isNull())
        setWorkingDirectory(process_spec->working_directory());

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
        mpl::log(mpl::Level::info, qPrintable(process_spec->program()), qPrintable(errorString()));
        return false;
    }

    return exitStatus() == QProcess::NormalExit && exitCode() == 0;
}

QString mp::Process::run_and_return_output(const QStringList& extra_arguments, const int timeout)
{
    start(extra_arguments);
    if (!waitForFinished(timeout))
    {
        mpl::log(mpl::Level::info, qPrintable(process_spec->program()), qPrintable(errorString()));
    }
    return readAllStandardOutput().trimmed();
}
