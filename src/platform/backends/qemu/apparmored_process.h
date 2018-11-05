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

#ifndef MULTIPASS_APPARMORED_PROCESS_H
#define MULTIPASS_APPARMORED_PROCESS_H

#include "apparmor.h"

#include <QProcess>

namespace multipass
{

class AppArmoredProcess : public QObject
{
    Q_OBJECT
public:
    AppArmoredProcess(const AppArmor &apparmor);
    virtual ~AppArmoredProcess();

    virtual QString program() const = 0;
    virtual QStringList arguments() const = 0;
    virtual QString apparmor_profile() const = 0;
    virtual QString identifier() const;

    QString workingDirectory() const;
    void setWorkingDirectory(const QString& dir);

    void start();
    void terminate();
    void kill();

    qint64 processId() const;
    QProcess::ProcessState state() const;

    bool waitForStarted(int msecs = 30000);
    bool waitForFinished(int msecs = 30000);

    qint64 write(const QByteArray& data);

    QByteArray readAllStandardOutput();
    QByteArray readAllStandardError();

signals:
    void started();
    void errorOccurred(QProcess::ProcessError error);
    void finished(int exit_code, QProcess::ExitStatus exit_status);
    void readyReadStandardOutput();
    void readyReadStandardError();
    void stateChanged(QProcess::ProcessState newState);

protected:
    const QString apparmor_profile_name() const;

private:
    const AppArmor &apparmor;
    QProcess process;
};

} // namespace multipass

#endif // MULTIPASS_APPARMORED_PROCESS_H
