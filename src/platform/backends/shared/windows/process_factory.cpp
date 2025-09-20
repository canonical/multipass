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

#include "process_factory.h"
#include <multipass/logging/log.h>
#include <multipass/process/basic_process.h>
#include <multipass/process/process_spec.h>
#include <multipass/process/simple_process_spec.h>

#include <fmt/format.h>
#include <windows.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const auto category = "process";

// Returns the last Win32 error, in string format. Returns an empty string if there is no error.
std::string GetLastErrorAsString()
{
    // Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return std::string(); // No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL,
                                 errorMessageID,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 (LPSTR)&messageBuffer,
                                 0,
                                 NULL);

    std::string message(messageBuffer, size);

    // Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

} // namespace

namespace
{
class WindowsProcess : public mp::BasicProcess
{
public:
    WindowsProcess(HANDLE ghJob, std::unique_ptr<mp::ProcessSpec>&& process_spec)
        : mp::BasicProcess(std::move(process_spec))
    {
        if (ghJob != nullptr)
        {
            connect(&process, &QProcess::started, [this, ghJob]() {
                auto pid = process.processId();
                if (0 == AssignProcessToJobObject(ghJob, OpenProcess(PROCESS_ALL_ACCESS, 0, pid)))
                {
                    mpl::warn(::category,
                              "Could not AssignProcessToObject the spawned process: {}",
                              GetLastErrorAsString());
                }
            });
        }

        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            // The CREATE_NEW_PROCESS_GROUP flag allows process to have its own signal
            // group separate from the parent. This way, the parent can send signals
            // e.g. (Ctrl-C) to the child for a graceful shutdown without having to
            // disrupt other children, or the parent process itself if it has a signal
            // handler. The constant is to avoid including windows.h & winbase.h.
            constexpr static unsigned long kCreateNewProcessGroup = 0x00000200;
            args->flags |= kCreateNewProcessGroup;
        });
    }

    void terminate() override
    {
        // Let's send the child a signal first to let it gracefully terminate.
        // the process_id() is also the console group id.
        if (GenerateConsoleCtrlEvent(/*dwCtrlEvent*/ CTRL_BREAK_EVENT,
                                     /*dwProcessGroupId*/ process_id()) == 0)
        {
            // failed to Ctrl+C, resort to killing
            mpl::warn(::category,
                      "Failed to Ctrl+Break, falling back to kill(). Error was: {}",
                      GetLastErrorAsString());
            kill();
        }
    }
};
} // namespace

mp::ProcessFactory::ProcessFactory(const Singleton<ProcessFactory>::PrivatePass& pass)
    : Singleton<ProcessFactory>::Singleton{pass}, ghJob{nullptr}
{
    /* Create a Windows Job Object that will ensure all child processes are collected on stop */
    BOOL alreadyInProcessJob;
    bool bSuccess = IsProcessInJob(GetCurrentProcess(), nullptr, &alreadyInProcessJob);
    if (!bSuccess)
    {
        mpl::warn(::category, "IsProcessInJob failed: error {}", GetLastErrorAsString());
    }
    if (alreadyInProcessJob)
    {
        mpl::warn(::category,
                  "Process is already in Job, spawned processes will not be cleaned up");
    }

    ghJob = CreateJobObject(nullptr, nullptr);
    if (ghJob == nullptr)
    {
        mpl::warn(::category, "Could not create job object: {}", GetLastErrorAsString());
    }
    else
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};

        // Configure all child processes associated with the job to terminate when the
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (0 ==
            SetInformationJobObject(ghJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
        {
            mpl::warn(::category, "Could not SetInformationJobObject: {}", GetLastErrorAsString());
        }
    }
}

std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(
    std::unique_ptr<mp::ProcessSpec>&& process_spec) const
{
    return std::make_unique<::WindowsProcess>(ghJob, std::move(process_spec));
}

std::unique_ptr<mp::Process> mp::ProcessFactory::create_process(const QString& command,
                                                                const QStringList& arguments) const
{
    return create_process(simple_process_spec(command, arguments));
}
