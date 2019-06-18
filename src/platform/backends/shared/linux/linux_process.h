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

#ifndef MULTIPASS_LINUXPROCESS_H
#define MULTIPASS_LINUXPROCESS_H

#include <multipass/process.h>
#include "process_spec.h"
#include <QProcess>
#include <memory>

namespace multipass
{

class LinuxProcess : public Process
{
    Q_OBJECT
public:
    virtual ~LinuxProcess() = default;

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;
    QProcessEnvironment process_environment() const override;

    void start(const QStringList& extra_arguments = QStringList()) override;
    void kill() override;

    bool wait_for_started(int msecs = 30000) override; // return false if process fails to start
    bool wait_for_finished(int msecs = 30000) override; // return false if wait times-out, or process never started

    bool running() const override;

    QByteArray read_all_standard_output() override;
    QByteArray read_all_standard_error() override;

    qint64 write(const QByteArray &data) override;

    bool run_and_return_status(const QStringList& extra_arguments = QStringList(), const int timeout = 30000) override;
    QString run_and_return_output(const QStringList& extra_arguments = QStringList(), const int timeout = 30000) override;

protected:
    LinuxProcess(std::unique_ptr<ProcessSpec>&& spec);
    const std::unique_ptr<ProcessSpec> process_spec;

    QProcess process; // ease testing

private:
    void run_and_wait_until_finished(const QStringList& extra_arguments, const int timeout);
};

} // namespace multipass

#endif // MULTIPASS_LINUXPROCESS_H
