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

#include <stdexcept>

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

    multipass mount <source> {}:<target>)";

constexpr auto server_timeout_message = R"(Timed out waiting for {}.
The instance was created but may still be starting. Check with:

    {}

Check its initialization status with:

    multipass exec {} -- cloud-init status)";

constexpr auto server_mount_timeout_message = R"(

No mounts were performed. Apply any missing mounts manually with:

    multipass mount <source> {}:<target>)";

auto display_vm_name(const std::string& vm_name) -> std::string_view
{
    if (vm_name.empty())
        return unknown_vm_name;

    return vm_name;
}

auto instance_status_command(const std::string& vm_name)
{
    return vm_name.empty() ? std::string{"multipass list"}
                           : fmt::format("multipass info {}", vm_name);
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
} // namespace

std::string LaunchUserMessages::client_timeout(const std::string& vm_name, bool has_mount_options)
{
    auto message = fmt::format(client_timeout_message, instance_status_command(vm_name));
    if (has_mount_options)
        message += fmt::format(client_mount_timeout_message, display_vm_name(vm_name));

    return message;
}

std::string LaunchUserMessages::server_timeout(ReadinessError::ErrorCode readiness_error,
                                               const std::string& vm_name,
                                               bool has_mount_options)
{
    const auto name = display_vm_name(vm_name);
    auto message = fmt::format(server_timeout_message,
                               readiness_stage_for(readiness_error),
                               instance_status_command(vm_name),
                               name);
    if (has_mount_options)
        message += fmt::format(server_mount_timeout_message, name);

    return message;
}
} // namespace multipass
