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

namespace mp = multipass;

mp::AppArmoredProcess::AppArmoredProcess(const mp::AppArmor &apparmor)
    : apparmor{apparmor}
{
    connect(&process, &QProcess::errorOccurred, this, &AppArmoredProcess::errorOccurred);
    connect(&process, &QProcess::started, this, &AppArmoredProcess::started);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &AppArmoredProcess::finished);
    connect(&process, &QProcess::readyReadStandardOutput, this, &AppArmoredProcess::readyReadStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &AppArmoredProcess::readyReadStandardError);
    connect(&process, &QProcess::stateChanged, this, &AppArmoredProcess::stateChanged);
}

mp::AppArmoredProcess::~AppArmoredProcess()
{
    // TODO: remove registered AA profile
}

// To distinguish multiple instances of the same application, use this identifier
QString mp::AppArmoredProcess::identifier() const
{
    return QString();
}

QString mp::AppArmoredProcess::workingDirectory() const
{
    return process.workingDirectory();
}

void mp::AppArmoredProcess::setWorkingDirectory(const QString& dir)
{
    process.setWorkingDirectory(dir);
}

void mp::AppArmoredProcess::start()
{
    apparmor.load_policy(apparmor_profile());

    // GERRY: what do I do if not running under snap?? Or AppArmor not available??
    //process.start("aa-exec", QStringList() << "-p" << apparmor_profile_name() << "--" << program() << arguments());
    // alternative is to hook into the QPRocess::stateChanged -> Starting signal (blocked), and call the apparmor  aa_change_onexec
    // which hopefully (pending race conditions) will attach to the correct exec call.
    process.start(program(), arguments());
}

void mp::AppArmoredProcess::terminate()
{
    process.terminate();
}

void mp::AppArmoredProcess::kill()
{
    process.kill();
}

qint64 mp::AppArmoredProcess::processId() const
{
    return process.processId();
}

QProcess::ProcessState mp::AppArmoredProcess::state() const
{
    return process.state();
}

bool mp::AppArmoredProcess::waitForStarted(int msecs)
{
    return process.waitForStarted(msecs);
}

bool mp::AppArmoredProcess::waitForFinished(int msecs)
{
    return process.waitForFinished(msecs);
}

qint64 mp::AppArmoredProcess::write(const QByteArray& data)
{
    return process.write(data);
}

QByteArray mp::AppArmoredProcess::readAllStandardOutput()
{
    return process.readAllStandardOutput();
}

QByteArray mp::AppArmoredProcess::readAllStandardError()
{
    return process.readAllStandardError();
}

const QString mp::AppArmoredProcess::apparmor_profile_name() const
{
    if (!identifier().isNull()) {
        return QStringLiteral("multipass.") + identifier() + '.' + program();
    } else {
        return QStringLiteral("multipass.") + program();
    }
}

// NEED Destructor to remove AA profile on close/stop
