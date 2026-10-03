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

#include <multipass/memory_size.h>
#include <multipass/network_interface.h>
#include <multipass/path.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_image.h>
#include <multipass/vm_mount.h>

#include <yaml-cpp/yaml.h>

#include <boost/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include <QMetaType>

namespace multipass
{
class VirtualMachineDescription
{
public:
    int num_cores;
    MemorySize mem_size;
    MemorySize disk_space;
    std::string vm_name;
    std::string zone;
    std::string default_mac_address;
    std::vector<NetworkInterface> extra_interfaces;
    std::string ssh_username;
    VMImage image;
    Path cloud_init_iso;
    YAML::Node meta_data_config;
    YAML::Node user_data_config;
    YAML::Node vendor_data_config;
    YAML::Node network_data_config;
    boost::json::object metadata;
    VirtualMachine::State state;
    int clone_count; // tracks the number of clones made from this VM (regardless of deletes)
    bool deleted;
    std::unordered_map<std::string, VMMount> mounts;
};

void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const VirtualMachineDescription& desc);
VirtualMachineDescription tag_invoke(const boost::json::value_to_tag<VirtualMachineDescription>&,
                                     const boost::json::value& json);
} // namespace multipass

Q_DECLARE_METATYPE(multipass::VirtualMachineDescription)
