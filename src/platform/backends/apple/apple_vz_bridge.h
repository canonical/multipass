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

#pragma once

#include <apple/cf_error.h>

#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>

#include <filesystem>

namespace multipass::apple
{
using VMHandle = std::shared_ptr<void>;

enum class AppleVMState
{
    stopped,
    running,
    paused,
    error,
    starting,
    pausing,
    resuming,
    stopping,
    saving,
    restoring
};

CFError init_with_configuration(const multipass::VirtualMachineDescription& desc,
                                VMHandle& out_handle);

// Starting and stopping VM
CFError start_with_completion_handler(const VMHandle& vm_handle);
CFError stop_with_completion_handler(const VMHandle& vm_handle);
CFError request_stop_with_error(const VMHandle& vm_handle);
CFError pause_with_completion_handler(const VMHandle& vm_handle);
CFError resume_with_completion_handler(const VMHandle& vm_handle);

// Saving and restoring VM
CFError save_machine_state_to_url(const VMHandle& vm_handle, const std::filesystem::path& path);
CFError restore_machine_state_from_url(const VMHandle& vm_handle,
                                       const std::filesystem::path& path);

// Getting VM state
AppleVMState get_state(const VMHandle& vm_handle);

// Validate the state of VM
bool can_start(const VMHandle& vm_handle);
bool can_pause(const VMHandle& vm_handle);
bool can_resume(const VMHandle& vm_handle);
bool can_stop(const VMHandle& vm_handle);
bool can_request_stop(const VMHandle& vm_handle);
} // namespace multipass::apple

template <>
struct fmt::formatter<multipass::apple::AppleVMState>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::apple::AppleVMState& state, FormatContext& ctx) const
    {
        std::string_view v = "(undefined)";
        switch (state)
        {
        case multipass::apple::AppleVMState::stopped:
            v = "stopped";
            break;
        case multipass::apple::AppleVMState::running:
            v = "running";
            break;
        case multipass::apple::AppleVMState::paused:
            v = "paused";
            break;
        case multipass::apple::AppleVMState::error:
            v = "error";
            break;
        case multipass::apple::AppleVMState::starting:
            v = "starting";
            break;
        case multipass::apple::AppleVMState::pausing:
            v = "pausing";
            break;
        case multipass::apple::AppleVMState::resuming:
            v = "resuming";
            break;
        case multipass::apple::AppleVMState::stopping:
            v = "stopping";
            break;
        case multipass::apple::AppleVMState::saving:
            v = "saving";
            break;
        case multipass::apple::AppleVMState::restoring:
            v = "restoring";
            break;
        default:
            v = "unknown";
            break;
        }
        return format_to(ctx.out(), "{}", v);
    }
};
