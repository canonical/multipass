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

#include <multipass/user_messages/launch_messages.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace multipass
{

namespace
{
constexpr auto unknown_vm_name = "<vm-name>";

constexpr auto client_timeout_message = R"(Timed out waiting for the launch operation.
The instance status is unknown and it may still be starting. Check with:

    {})";

constexpr auto client_mount_timeout_message = R"(

Mounts may not have been performed. Apply any missing mounts manually with:

)";

constexpr auto server_timeout_message = R"(Timed out waiting for {}.
The instance was created but may still be starting. Check with:

    {}

Check its initialization status with:

    multipass exec {} -- cloud-init status)";

constexpr auto server_mount_timeout_message = R"(

No mounts were performed. Apply any missing mounts manually with:

)";

std::string_view display_vm_name(std::string_view vm_name)
{
    return vm_name.empty() ? unknown_vm_name : vm_name;
}

std::string instance_status_command(std::string_view vm_name)
{
    return vm_name.empty() ? "multipass list" : fmt::format("multipass info {}", vm_name);
}

auto readiness_stage_for(ReadinessError::ErrorCode readiness_error)
{
    switch (readiness_error)
    {
    case ReadinessError::SSH_TIMEOUT:
        return "SSH";
    case ReadinessError::CLOUD_INIT_TIMEOUT:
        return "cloud-init";
    default:
        throw std::invalid_argument{"Readiness error does not describe a timeout"};
    }
}

bool nice_path(const std::string& p)
{
    return std::ranges::all_of(p, [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '/' || c == ':' ||
               c == std::filesystem::path::preferred_separator;
    });
}

bool nice_route(const LaunchUserMessages::MountRoute& mount_route)
{
    return nice_path(mount_route.source) && nice_path(mount_route.target);
}

bool nice_routes(const LaunchUserMessages::MountRoutes& mount_routes)
{
    return std::ranges::all_of(mount_routes, nice_route);
}

void append_mount_commands(fmt::memory_buffer& message,
                           const LaunchUserMessages::MountRoutes& mount_routes,
                           std::string_view vm_name)
{
    auto out = std::back_inserter(message);
    // Ideally we should shell escape the paths here. But instead we opt to show generic message if
    // we're uncertain about the content of the path. Especially since shell escaping depends on the
    // shell being used: bash / cmd / powershell.
    if (nice_routes(mount_routes))
    {
        for (const auto& [source, target] : mount_routes)
            fmt::format_to(out, "    multipass mount \"{}\" {}:\"{}\"\n", source, vm_name, target);
    }
    else
    {
        fmt::format_to(out, "    multipass mount <source> {}:<target>\n", vm_name);
    }
}

} // namespace

std::string LaunchUserMessages::client_timeout(std::string_view vm_name,
                                               const MountRoutes& mount_routes)
{
    fmt::memory_buffer message;
    auto out = std::back_inserter(message);

    fmt::format_to(out, client_timeout_message, instance_status_command(vm_name));
    if (!mount_routes.empty())
    {
        fmt::format_to(out, client_mount_timeout_message);
        append_mount_commands(message, mount_routes, display_vm_name(vm_name));
    }

    return fmt::to_string(message);
}

std::string LaunchUserMessages::server_timeout(ReadinessError::ErrorCode readiness_error,
                                               std::string_view vm_name,
                                               const MountRoutes& mount_routes)
{
    const auto name = display_vm_name(vm_name);
    fmt::memory_buffer message;
    auto out = std::back_inserter(message);
    fmt::format_to(out,
                   server_timeout_message,
                   readiness_stage_for(readiness_error),
                   instance_status_command(vm_name),
                   name);
    if (!mount_routes.empty())
    {
        fmt::format_to(out, server_mount_timeout_message);
        append_mount_commands(message, mount_routes, name);
    }

    return fmt::to_string(message);
}
} // namespace multipass
