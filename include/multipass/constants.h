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

#ifndef MULTIPASS_CONSTANTS_H
#define MULTIPASS_CONSTANTS_H

namespace multipass
{
constexpr auto client_name = "multipass";
constexpr auto daemon_name = "multipassd";

constexpr auto min_memory_size = "128M";
constexpr auto min_disk_size = "512M";
constexpr auto min_cpu_cores = "1";

constexpr auto default_memory_size = "1G";
constexpr auto default_disk_size = "5G";
constexpr auto default_cpu_cores = min_cpu_cores;

constexpr auto home_automount_dir = "Home";

constexpr auto driver_env_var = "MULTIPASS_VM_DRIVER";

constexpr auto winterm_profile_guid =
    "{aaaa9e6d-1e09-4be6-b76c-82b4ba1885fb}"; // identifies the primary Multipass profile in Windows Terminal

constexpr auto petenv_key = "client.primary-name";     // This will eventually be moved to some dynamic settings schema
constexpr auto driver_key = "local.driver";            // idem
constexpr auto autostart_key = "client.gui.autostart"; // idem
constexpr auto winterm_key = "client.windows-terminal.add-profiles"; // idem
} // namespace multipass

#endif // MULTIPASS_CONSTANTS_H
