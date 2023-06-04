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

#ifndef MULTIPASS_MOCK_PROCESS_FACTORY_H
#define MULTIPASS_MOCK_PROCESS_FACTORY_H

#include "common.h"
#include "process_factory.h" // rely on build system to include the right implementation

#include <multipass/process/process.h>

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
    std::optional<Callback> callback;
};

class MockProcess : public Process
{
public:
    MOCK_METHOD(void, start, (), (override));
    MOCK_METHOD(void, terminate, (), (override));
    MOCK_METHOD(void, kill, (), (override));
    MOCK_METHOD(bool, running, (), (const, override));
    MOCK_METHOD(ProcessState, process_state, (), (const, override));
    MOCK_METHOD(QString, error_string, (), (const, override));
    MOCK_METHOD(qint64, process_id, (), (const, override));
    MOCK_METHOD(ProcessState, execute, (int), (override));
    MOCK_METHOD(qint64, write, (const QByteArray&), (override));
    MOCK_METHOD(bool, wait_for_started, (int msecs), (override));
    MOCK_METHOD(bool, wait_for_finished, (int msecs), (override));

    MockProcess(std::unique_ptr<ProcessSpec>&& spec, std::vector<MockProcessFactory::ProcessInfo>& process_list);

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;
    QProcessEnvironment process_environment() const override;

    bool wait_for_ready_read(int msecs = 30000) override;
    MOCK_METHOD(QByteArray, read_all_standard_output, (), (override));
    MOCK_METHOD(QByteArray, read_all_standard_error, (), (override));
    void close_write_channel() override;
    void set_process_channel_mode(QProcess::ProcessChannelMode) override;
    void setup_child_process() override;

private:
    const std::unique_ptr<ProcessSpec> spec;
    ProcessState success_exit_state;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_PROCESS_FACTORY_H
