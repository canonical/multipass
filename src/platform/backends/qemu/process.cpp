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

namespace mp = multipass;

mp::Process::Process(std::unique_ptr<ProcessSpec> &&process_spec) : process_spec{std::move(process_spec)}
{
    connect(&process, &QProcess::errorOccurred, this, &Process::errorOccurred);
    connect(&process, &QProcess::started, this, &Process::started);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &Process::finished);
    connect(&process, &QProcess::readyReadStandardOutput, this, &Process::readyReadStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &Process::readyReadStandardError);
    connect(&process, &QProcess::stateChanged, this, &Process::stateChanged);
}

mp::Process::~Process()
{
}

void mp::Process::start()
{
    process.start(process_spec->program(), process_spec->arguments());
}

QString mp::Process::workingDirectory() const
{
    return process.workingDirectory();
}

QString mp::Process::program() const
{
    return process.program();
}

QStringList mp::Process::arguments() const
{
    return process.arguments();
}

void mp::Process::setWorkingDirectory(const QString& dir)
{
    process.setWorkingDirectory(dir);
}

void mp::Process::terminate()
{
    process.terminate();
}

void mp::Process::kill()
{
    process.kill();
}

qint64 mp::Process::processId() const
{
    return process.processId();
}

QProcess::ProcessState mp::Process::state() const
{
    return process.state();
}

bool mp::Process::waitForStarted(int msecs)
{
    return process.waitForStarted(msecs);
}

bool mp::Process::waitForFinished(int msecs)
{
    return process.waitForFinished(msecs);
}

qint64 mp::Process::write(const QByteArray& data)
{
    return process.write(data);
}

QByteArray mp::Process::readAllStandardOutput()
{
    return process.readAllStandardOutput();
}

QByteArray mp::Process::readAllStandardError()
{
    return process.readAllStandardError();
}
