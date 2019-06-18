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

#ifndef MULTIPASS_PROCESS_H
#define MULTIPASS_PROCESS_H

#include <memory>
#include <QStringList>
#include <QProcessEnvironment>

namespace multipass
{

class Process : public QObject
{
    Q_OBJECT
public:
    using UPtr = std::unique_ptr<Process>;

    virtual ~Process() = default;

    virtual QString program() const = 0;
    virtual QStringList arguments() const = 0;
    virtual QString working_directory() const = 0;
    virtual QProcessEnvironment process_environment() const = 0;

    virtual void start(const QStringList& extra_arguments = QStringList()) = 0;
    virtual void kill() = 0;

    virtual bool wait_for_started(int msecs = 30000) = 0; // return false if process fails to start
    virtual bool wait_for_finished(int msecs = 30000) = 0; // return false if wait times-out, or process never started

    virtual bool running() const = 0;

    virtual QByteArray read_all_standard_output() = 0;
    virtual QByteArray read_all_standard_error() = 0;

    virtual qint64 write(const QByteArray &data) = 0;

    virtual bool run_and_return_status(const QStringList& extra_arguments = QStringList(), const int timeout = 30000) = 0;
    virtual QString run_and_return_output(const QStringList& extra_arguments = QStringList(), const int timeout = 30000) = 0;

signals:
    void started();
    void finished(int exitCode, QProcess::ExitStatus exit_status); // normal or crash
    void error_occurred(QProcess::ProcessError error); // not running, starting, running
    void state_changed(QProcess::ProcessState state); // FailedToStart (file not found / resource error) Crashed, Timedout, ReadError, WriteError, UnknownError

    void ready_read_standard_output();
    void ready_read_standard_error();
};
} // namespace multipass
#endif // MULTIPASS_PROCESS_H
