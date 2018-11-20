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

#ifndef MULTIPASS_PROCESS_H
#define MULTIPASS_PROCESS_H

#include "process_spec.h"
#include <QProcess>

#include <memory>

namespace multipass
{

class Process : public QObject
{
    Q_OBJECT
public:
    virtual ~Process();

    virtual void start(const QStringList& extra_arguments = QStringList()) = 0;

    bool run_and_return_status(const QStringList& extra_arguments = QStringList(), const int timeout = 30000);
    QString run_and_return_output(const QStringList& extra_arguments = QStringList(), const int timeout = 30000);

    void terminate();
    void kill();

    QString workingDirectory() const;
    void setWorkingDirectory(const QString& dir);

    QString program() const;
    QStringList arguments() const;

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
    Process(std::unique_ptr<ProcessSpec>&& process_spec);

    const std::unique_ptr<ProcessSpec> process_spec;

    void start_process(const QString& program, const QStringList& arguments);

private:
    QProcess process;
};

} // namespace multipass

#endif // MULTIPASS_PROCESS_H
