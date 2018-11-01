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

#include "apparmored_process.h"

AppArmoredProcess::AppArmoredProcess()
{
    connect(&process, &QProcess::errorOccurred, this, &AppArmoredProcess::errorOccurred);
    connect(&process, &QProcess::started, this, &AppArmoredProcess::started);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &AppArmoredProcess::finished);
    connect(&process, &QProcess::readyReadStandardOutput, this, &AppArmoredProcess::readyReadStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &AppArmoredProcess::readyReadStandardError);
    connect(&process, &QProcess::stateChanged, this, &AppArmoredProcess::stateChanged);
}

AppArmoredProcess::~AppArmoredProcess()
{
    // TODO: remove registered AA profile
}

// To distinguish multiple instances of the same application, use this identifier
QString AppArmoredProcess::identifier() const
{
    return QString();
}

QString AppArmoredProcess::workingDirectory() const
{
    return process.workingDirectory();
}

void AppArmoredProcess::setWorkingDirectory(const QString& dir)
{
    process.setWorkingDirectory(dir);
}

void AppArmoredProcess::start()
{
    // TODO - write the apparmor profile to disk
    // Install the profile into the kernel, named apparmor_profile_name()

    // GERRY: what do I do if not running under snap?? Or AppArmor not available??
    //process.start("aa-exec", QStringList() << "-p" << apparmor_profile_name() << "--" << program() << arguments());
    process.start(program(), arguments());
}

void AppArmoredProcess::terminate()
{
    process.terminate();
}

void AppArmoredProcess::kill()
{
    process.kill();
}

qint64 AppArmoredProcess::processId() const
{
    return process.processId();
}

QProcess::ProcessState AppArmoredProcess::state() const
{
    return process.state();
}

bool AppArmoredProcess::waitForStarted(int msecs)
{
    return process.waitForStarted(msecs);
}

bool  AppArmoredProcess::waitForFinished(int msecs)
{
    return process.waitForFinished(msecs);
}

qint64 AppArmoredProcess::write(const QByteArray& data)
{
    return process.write(data);
}

QByteArray AppArmoredProcess::readAllStandardOutput()
{
    return process.readAllStandardOutput();
}

QByteArray AppArmoredProcess::readAllStandardError()
{
    return process.readAllStandardError();
}

const QString AppArmoredProcess::apparmor_profile_name() const
{
    if (!identifier().isNull()) {
        return QStringLiteral("multipass.") + identifier() + '.' + program();
    } else {
        return QStringLiteral("multipass.") + program();
    }
}

// NEED Destructor to remove AA profile on close/stop
