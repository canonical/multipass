/*
 * Copyright (C) Canonical, Ltd.
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

#ifndef MULTIPASS_BASIC_PROCESS_H
#define MULTIPASS_BASIC_PROCESS_H

#include <multipass/process/process.h>
#include <multipass/process/process_spec.h>

#include <memory>

namespace multipass
{
class CustomQProcess;

// BasicProcess implements the Process interface without using any platform-specifics.

class BasicProcess : public Process
{
    Q_OBJECT
public:
    BasicProcess(std::shared_ptr<ProcessSpec> spec);
    virtual ~BasicProcess();

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;
    QProcessEnvironment process_environment() const override;
    virtual qint64 process_id() const override;

    void start() override;
    void terminate() override;
    void kill() override;

    bool wait_for_started(int msecs = 30000) override;
    bool wait_for_finished(int msecs = 30000) override;
    bool wait_for_ready_read(int msecs = 30000) override;

    bool running() const override;
    ProcessState process_state() const override;
    QString error_string() const override;

    QByteArray read_all_standard_output() override;
    QByteArray read_all_standard_error() override;

    qint64 write(const QByteArray& data) override;
    void close_write_channel() override;
    void set_process_channel_mode(QProcess::ProcessChannelMode mode) override;

    ProcessState execute(const int timeout = 30000) override;

protected:
    const std::shared_ptr<ProcessSpec> process_spec;

    void setup_child_process() override;

    class CustomQProcess : public QProcess
    {
    public:
        CustomQProcess(BasicProcess* p);
    };

    CustomQProcess process; // ease testing

private:
    void handle_started();
    void run_and_wait_until_finished(const int timeout);
    qint64 pid = 0;
};

} // namespace multipass

#endif // MULTIPASS_BASIC_PROCESS_H
