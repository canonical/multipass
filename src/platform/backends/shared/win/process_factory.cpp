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

#include "process_factory.h"
#include "basic_process.h"
#include <multipass/logging/log.h>
#include <multipass/process_spec.h>

#include <fmt/format.h>
#include <windows.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const auto category = "process";
}

namespace
{
class WindowsProcess : public mp::BasicProcess
{
public:
    WindowsProcess(HANDLE ghJob, std::unique_ptr<mp::ProcessSpec>&& process_spec)
        : mp::BasicProcess(process_spec->error_log_level())
    {
        setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments* args) { args->flags = CREATE_BREAKAWAY_FROM_JOB; });
        connect(this, &QProcess::started, [this, ghJob]() {
            PROCESS_INFORMATION* processInfo = pid();
            if (0 == AssignProcessToJobObject(ghJob, processInfo->hProcess))
            {
                mpl::log(mpl::Level::warning, ::category, "Could not AssignProcessToObject the spawned process");
            }
        });
    }
};
} // namespace

mp::ProcessFactory::ProcessFactory()
{
    /* Create a Windows Job Object that will ensure all child processes are collected on stop */
    BOOL alreadyInProcessJob;
    bool bSuccess = IsProcessInJob(GetCurrentProcess(), nullptr, &alreadyInProcessJob);
    if (!bSuccess)
    {
        mpl::log(mpl::Level::warning, ::category, fmt::format("IsProcessInJob failed: error {}", GetLastError()));
    }
    if (alreadyInProcessJob)
    {
        mpl::log(mpl::Level::warning, ::category,
                 "Process is already in Job, spawned processes will not be cleaned up");
    }

    ghJob = CreateJobObject(nullptr, nullptr);
    if (ghJob == nullptr)
    {
        mpl::log(mpl::Level::warning, ::category, "Could not create job object");
    }
    else
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};

        // Configure all child processes associated with the job to terminate when the
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (0 == SetInformationJobObject(ghJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli)))
        {
            mpl::log(mpl::Level::warning, ::category, "Could not SetInformationJobObject");
        }
    }
}

std::unique_ptr<mp::Process>
mp::ProcessFactory::create_process(std::unique_ptr<mp::ProcessSpec>&& process_spec) const
{
    return std::make_unique<::WindowsProcess>(ghJob, std::move(process_spec));
}
