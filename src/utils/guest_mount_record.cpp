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
#include <multipass/virtual_machine.h>

namespace mp = multipass;

namespace
{
std::string encode_path(const std::string& path)
{
    return path; // TODO@ricab
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
    return "TODO"; // TODO@ricab
}

std::string mp::GuestMountRecord::make_erase_cmd() const
{
    return "TODO"; // TODO@ricab
}

void mp::GuestMountRecord::deploy(VirtualMachine& vm) const
{
    vm.ssh_exec(deploy_cmd);
}

void mp::GuestMountRecord::erase(VirtualMachine& vm) const
{
    vm.ssh_exec(erase_cmd);
}
