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

#ifndef MULTIPASS_FAKE_LINUX_PROCESS_FACTORY_H
#define MULTIPASS_FAKE_LINUX_PROCESS_FACTORY_H

#include <src/platform/backends/shared/linux/linux_process.h>
#include <src/platform/backends/shared/linux/process_factory.h>

namespace multipass
{
namespace test
{

struct QProcessInfo
{
    QString program;
    QStringList arguments;
    QProcess::ProcessState state;
};

// FakeLinuxProcess pretends that it launches a long-running process
class FakeLinuxProcess : public LinuxProcess
{
public:
    FakeLinuxProcess(std::unique_ptr<ProcessSpec>&& spec, std::vector<QProcessInfo>& current_processes)
        : LinuxProcess(std::move(spec))
    {
        process_info.program = process.program();
        process_info.arguments = process.arguments();
        process_info.state = QProcess::ProcessState::NotRunning;
        current_processes.emplace_back(process_info);
    }

    void start(const QStringList& extra_args = QStringList())
    {
        process_info.arguments = process.arguments() << extra_args;
        process_info.state = QProcess::ProcessState::Running;
        emit started();
    }
    void kill()
    {
        process_info.state = QProcess::ProcessState::NotRunning;
        emit finished(0, QProcess::NormalExit);
    }

    bool wait_for_started(int)
    {
        return true;
    }
    bool wait_for_finished(int)
    {
        return true;
    }

    bool running() const
    {
        return true;
    }

    bool run_and_return_status(const QStringList&, const int)
    {
        return true;
    }
    QString run_and_return_output(const QStringList&, const int)
    {
        return "";
    }

    QProcessInfo process_info;
};

class FakeLinuxProcessFactory : public ProcessFactory
{
public:
    std::vector<QProcessInfo> created_processes;

    std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&& spec) const override
    {
        return std::make_unique<FakeLinuxProcess>(std::move(spec),
                                                  const_cast<std::vector<QProcessInfo>&>(created_processes));
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_FAKE_LINUX_PROCESS_FACTORY_H
