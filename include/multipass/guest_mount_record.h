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

#include <multipass/disabled_copy_move.h>

#include <string>

namespace multipass
{
class VirtualMachine;

// A guest's record of a mount, deployed as a file that can be accessed by a motd fragment to
// list active mounts at login
class GuestMountRecord : private DisabledCopyMove
{
public:
    GuestMountRecord(const std::string& source, const std::string& target);

    void deploy(VirtualMachine& vm) const;
    void undeploy(VirtualMachine& vm) const;

private:
    std::string make_deploy_cmd() const;
    std::string make_undeploy_cmd() const;

    const std::string source;         // keeping this around to ease debugging
    const std::string encoded_target; // idem
    const std::string deploy_cmd;
    const std::string undeploy_cmd;
};
} // namespace multipass
