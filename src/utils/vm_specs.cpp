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

#include <multipass/constants.h>
#include <multipass/exceptions/ghost_instance_exception.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

namespace mp = multipass;
namespace mpu = multipass::utils;

void mp::tag_invoke(const boost::json::value_from_tag&,
                    boost::json::value& json,
                    const mp::VMSpecs& specs)
{
    json = {
        {"state", static_cast<int>(specs.state)},
        {"deleted", specs.deleted},
        {"metadata", specs.metadata},
        {"mounts", boost::json::value_from(specs.mounts, MapAsJsonArray{"target_path"})},
        {"clone_count", specs.clone_count},
    };
}

mp::VMSpecs mp::tag_invoke(const boost::json::value_to_tag<mp::VMSpecs>&,
                           const boost::json::value& json)
{
    auto deleted = value_to<bool>(json.at("deleted"));
    auto metadata = json.at("metadata").as_object();

    // Ghost records predate vm-description.json, so they still carry the legacy keys
    if (!lookup_or<int>(json, "num_cores", 0) && !deleted &&
        lookup_or<std::string>(json, "ssh_username", "").empty() && metadata.empty() &&
        !MemorySize{lookup_or<std::string>(json, "mem_size", "")}.in_bytes() &&
        !MemorySize{lookup_or<std::string>(json, "disk_space", "")}.in_bytes())
        throw GhostInstanceException();

    using mounts_t = std::unordered_map<std::string, VMMount>;
    return {
        static_cast<mp::VirtualMachine::State>(value_to<int>(json.at("state"))),
        value_to<mounts_t>(json.at("mounts"), MapAsJsonArray{"target_path"}),
        deleted,
        metadata,
        lookup_or<int>(json, "clone_count", 0),
    };
}
