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

#ifndef MULTIPASS_YAML_NODE_UTILS_H
#define MULTIPASS_YAML_NODE_UTILS_H

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

namespace multipass
{
struct NetworkInterface;
namespace utils
{
// yaml helpers
std::string emit_yaml(const YAML::Node& node);
std::string emit_cloud_config(const YAML::Node& node);
// when file_content is non-empty, make_cloud_init_meta_config constructs the node based on the string and replaces
// the original name occurrences with the input name
YAML::Node make_cloud_init_meta_config(const std::string& name, const std::string& file_content = std::string{});
// load the file_content to construct the node and overwrite the instance-id, when the new_instance_id is provided, then
// it is used, if not, then there will be a generated new instance id to be used
YAML::Node make_cloud_init_meta_config_with_id_tweak(const std::string& file_content,
                                                     const std::string& new_instance_id = std::string());
// when file_content is non-empty, make_cloud_init_network_config constructs the node based on the string and replaces
// the default mac address and extra interfaces
YAML::Node make_cloud_init_network_config(const std::string& default_mac_addr,
                                          const std::vector<NetworkInterface>& extra_interfaces,
                                          const std::string& file_content = std::string{});
// adds one extra interface to the network_config_file_content baseline, it creats the default address node
// together with the extra interface node when it is empty,
YAML::Node add_extra_interface_to_network_config(const std::string& default_mac_addr,
                                                 const NetworkInterface& extra_interface,
                                                 const std::string& network_config_file_content);
// the make_cloud_init_network_config and add_extra_interface_to_network_config functions are adapted to generate the
// new format network-config file (having default interface present and having dhcp-identifier: mac on every network
// interface). At the same time, it also needs to take care of the pre-existed file, meaning that the generated file
// from the old file should have the new format.
} // namespace utils
} // namespace multipass
#endif // MULTIPASS_YAML_NODE_UTILS_H
