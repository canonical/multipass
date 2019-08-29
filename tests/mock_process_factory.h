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

#ifndef MULTIPASS_MOCK_PROCESS_FACTORY_H
#define MULTIPASS_MOCK_PROCESS_FACTORY_H

#include <gmock/gmock.h>
#include <multipass/optional.h>
#include <multipass/process.h>
#include <src/platform/backends/shared/linux/process_factory.h>

#include <functional>

using namespace testing;

namespace multipass
{
namespace test
{
class MockProcess;

class MockProcessFactory : public ProcessFactory
{
public:
    struct ProcessInfo
    {
        QString command;
        QStringList arguments;
    };
    using Callback = std::function<void(MockProcess*)>;

    // MockProcessFactory installed with Inject() call, and uninstalled when the Scope object deleted
    struct Scope
    {
        ~Scope();

        // Get info about the Processes launched with this list
        std::vector<ProcessInfo> process_list();

        // To mock the created Process, register a callback to be called on each Process creation
        // Only one callback is supported
        void register_callback(const Callback& callback);
    };

    static std::unique_ptr<Scope> Inject();

    // Implementation
    using ProcessFactory::ProcessFactory;

    std::unique_ptr<Process> create_process(std::unique_ptr<ProcessSpec>&& spec) const override;

private:
    static MockProcessFactory& mock_instance();
    void register_callback(const Callback& callback);
    std::vector<ProcessInfo> process_list;
    multipass::optional<Callback> callback;
};

class MockProcess : public Process
{
public:
    MOCK_METHOD0(start, void());
    MOCK_METHOD0(kill, void());
    MOCK_CONST_METHOD0(running, bool());
    MOCK_CONST_METHOD0(process_state, ProcessState());
    MOCK_METHOD1(execute, ProcessState(int));

    MockProcess(std::unique_ptr<ProcessSpec>&& spec, std::vector<MockProcessFactory::ProcessInfo>& process_list);

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;
    QProcessEnvironment process_environment() const override;

    bool wait_for_started(int msecs = 30000) override;
    bool wait_for_finished(int msecs = 30000) override;
    MOCK_METHOD0(read_all_standard_output, QByteArray());
    MOCK_METHOD0(read_all_standard_error, QByteArray());
    qint64 write(const QByteArray& data) override;
    void close_write_channel() override;
    void setup_child_process() override;

private:
    const std::unique_ptr<ProcessSpec> spec;
    ProcessState success_exit_state;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_PROCESS_FACTORY_H
