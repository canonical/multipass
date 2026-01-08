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

#include <chrono>
#include <initializer_list>

using namespace std::chrono_literals;

namespace multipass
{
constexpr auto client_name = "multipass";
constexpr auto daemon_name = "multipassd";

constexpr auto release_remote = "release";
constexpr auto daily_remote = "daily";
constexpr auto snapcraft_remote = "snapcraft";
constexpr auto core_remote = "core";

constexpr auto min_memory_size = "128M";
constexpr auto min_disk_size = "512M";
constexpr auto min_cpu_cores = "1";

constexpr auto default_memory_size = "1G";
constexpr auto default_disk_size = "5G";
constexpr auto default_cpu_cores = min_cpu_cores;
constexpr auto default_timeout = std::chrono::seconds(300);
constexpr auto image_resize_timeout =
    std::chrono::duration_cast<std::chrono::milliseconds>(5min).count();

constexpr auto home_automount_dir = "Home";

constexpr auto multipass_storage_env_var = "MULTIPASS_STORAGE";
constexpr auto driver_env_var = "MULTIPASS_VM_DRIVER";
constexpr auto distributions_url_env_var = "MULTIPASS_DISTRIBUTIONS_URL";

constexpr auto winterm_profile_guid =
    "{aaaa9e6d-1e09-4be6-b76c-82b4ba1885fb}"; // identifies the primary Multipass profile in Windows
                                              // Terminal

constexpr auto bridged_network_name = "bridged";

constexpr auto settings_extension = ".conf";
constexpr auto daemon_settings_root = "local";

// Settings keys (might eventually be moved to some dynamic settings schema)
constexpr auto petenv_key = "client.primary-name";
constexpr auto driver_key = "local.driver";
constexpr auto passphrase_key = "local.passphrase";
constexpr auto bridged_interface_key = "local.bridged-network";
constexpr auto mounts_key = "local.privileged-mounts";
constexpr auto winterm_key = "client.apps.windows-terminal.profiles";
constexpr auto mirror_key = "local.image.mirror"; // the mirror of simple streams

constexpr auto cloud_init_file_name = "cloud-init-config.iso";

[[maybe_unused]] // hands off clang-format
constexpr auto key_examples = {petenv_key, driver_key, mounts_key};
constexpr auto petenv_default = "primary";
constexpr auto timeout_exit_code = 5;
constexpr auto authenticated_certs_dir = "authenticated-certs";
constexpr auto home_in_instance = "/home/ubuntu";

constexpr std::chrono::milliseconds vm_shutdown_timeout =
    300000ms; // unit: ms, 5 minute timeout for shutdown/suspend
constexpr auto default_ssh_port = 22;
} // namespace multipass
