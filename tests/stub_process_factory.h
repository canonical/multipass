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

#ifndef MULTIPASS_STUB_PROCESS_FACTORY_H
#define MULTIPASS_STUB_PROCESS_FACTORY_H

#include <multipass/process.h>
#include <src/platform/backends/shared/linux/process_factory.h>

namespace multipass
{
namespace test
{
class StubProcess : public Process
{
public:
    StubProcess()
    {
    }

    QString program() const
    {
        return "";
    }
    QStringList arguments() const
    {
        return {};
    }
    QString working_directory() const
    {
        return "";
    }
    QProcessEnvironment process_environment() const
    {
        return QProcessEnvironment();
    }

    void start(const QStringList& extra_arguments = QStringList())
    {
    }
    void kill()
    {
    }

    bool wait_for_started(int msecs = 30000)
    {
        return true;
    }
    bool wait_for_finished(int msecs = 30000)
    {
        return true;
    }

    bool running() const
    {
        return true;
    }

    QByteArray read_all_standard_output()
    {
        return "";
    }
    QByteArray read_all_standard_error()
    {
        return "";
    }

    qint64 write(const QByteArray& data)
    {
        return 0;
    }

    bool run_and_return_status(const QStringList& extra_arguments = QStringList(), const int timeout = 30000)
    {
        return true;
    }
    QString run_and_return_output(const QStringList& extra_arguments = QStringList(), const int timeout = 30000)
    {
        return "";
    }
};

class StubProcessFactory : public ProcessFactory
{
public:
    using ProcessFactory::ProcessFactory;

    std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&&) const override;

    static StubProcessFactory& stub_instance();
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_PROCESS_FACTORY_H
