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

#include <multipass/guest_mount_record.h>

#include <multipass/format.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

namespace mp = multipass;
namespace mpu = mp::utils;

namespace
{
constexpr auto records_dir = "/run/multipass/mounts";

// Encode a path as a unique, reversible file name, by replacing slashes with %2F and % (the escape
// character) with %25
std::string encode_path(const std::string& path)
{
    std::string ret;
    for (const auto c : path)
    {
        if (c == '%')
            ret += "%25";
        else if (c == '/')
            ret += "%2F";
        else
            ret += c;
    }

    return ret;
}

std::string escaped_record_path(const std::string& encoded_target)
{
    return mpu::escape_for_shell(fmt::format("{}/{}", records_dir, encoded_target));
}
} // namespace

mp::GuestMountRecord::GuestMountRecord(const std::string& source, const std::string& target)
    : source{source},
      encoded_target{encode_path(target)},
      deploy_cmd{make_deploy_cmd()},
      erase_cmd{make_erase_cmd()}
{
}

std::string mp::GuestMountRecord::make_deploy_cmd() const
{
    static constexpr auto deploy_cmd_template =
        "sudo mkdir -p {} && printf '%s\\n' {} | sudo tee {} >/dev/null";

    return fmt::format(deploy_cmd_template,
                       records_dir,
                       mpu::escape_for_shell(source),
                       escaped_record_path(encoded_target));
}

std::string mp::GuestMountRecord::make_erase_cmd() const
{
    static constexpr auto erase_cmd_template = "sudo rm -f {}";

    return fmt::format(erase_cmd_template, escaped_record_path(encoded_target));
}

void mp::GuestMountRecord::deploy(VirtualMachine& vm) const
{
    vm.ssh_exec(deploy_cmd);
}

void mp::GuestMountRecord::erase(VirtualMachine& vm) const
{
    vm.ssh_exec(erase_cmd);
}
