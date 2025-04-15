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

#include <multipass/format.h>
#include <multipass/network_interface.h>
#include <multipass/yaml_node_utils.h>

#include <cassert>

namespace mp = multipass;

struct interface_details
{
    std::string name;
    std::string mac_addr;
    std::optional<int> route_metric{std::nullopt};
    bool optional{false};
};

namespace YAML
{
template <>
struct convert<interface_details>
{
    static Node encode(const interface_details& iface)
    {
        Node node;
        node["match"]["macaddress"] = iface.mac_addr;
        node["dhcp4"] = true;
        node["dhcp-identifier"] = "mac";
        // We make the default gateway associated with the first interface.
        if (iface.route_metric)
        {
            node["dhcp4-overrides"]["route-metric"] = iface.route_metric.value();
        }
        // Make the interface optional, which means that networkd will not wait for the device to be configured.

        if (iface.optional)
        {
            node["optional"] = true;
        }
        node["set-name"] = iface.name;
        return node;
    }
};
} // namespace YAML

constexpr static std::string_view kDefaultInterfaceName = "primary";
constexpr static std::string_view kExtraInterfaceNamePattern = "extra{}";

static auto make_default_interface(const std::string& mac_addr)
{
    interface_details iface{};
    iface.name = kDefaultInterfaceName;
    iface.mac_addr = mac_addr;
    return iface;
}

static auto make_extra_interface(const std::string& mac_addr, std::uint32_t extra_interface_idx = 0)
{
    interface_details iface{};
    iface.name = fmt::format(kExtraInterfaceNamePattern, extra_interface_idx);
    iface.mac_addr = mac_addr;
    iface.optional = true;
    iface.route_metric = 200;
    return iface;
}

std::string mp::utils::emit_yaml(const YAML::Node& node)
{
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << node;
    if (!emitter.good())
        throw std::runtime_error{fmt::format("Failed to emit YAML: {}", emitter.GetLastError())};

    emitter << YAML::Newline;
    return emitter.c_str();
}

std::string mp::utils::emit_cloud_config(const YAML::Node& node)
{
    return fmt::format("#cloud-config\n{}", emit_yaml(node));
}

YAML::Node mp::utils::make_cloud_init_meta_config(const std::string& name, const std::string& file_content)
{
    YAML::Node meta_data = file_content.empty() ? YAML::Node{} : YAML::Load(file_content);

    if (!file_content.empty())
    {
        const std::string old_hostname = meta_data["local-hostname"].as<std::string>();
        std::string old_instance_id = meta_data["instance-id"].as<std::string>();

        // The assumption here is that the instance_id is the hostname optionally appended _e sequence
        assert(old_instance_id.size() >= old_hostname.size());
        // replace the old host name with the new host name
        meta_data["instance-id"] = old_instance_id.replace(0, old_hostname.size(), name);
    }
    else
    {
        meta_data["instance-id"] = name;
    }

    meta_data["local-hostname"] = name;
    meta_data["cloud-name"] = "multipass";

    return meta_data;
}

YAML::Node mp::utils::make_cloud_init_meta_config_with_id_tweak(const std::string& file_content,
                                                                const std::string& new_instance_id)
{
    YAML::Node meta_data = YAML::Load(file_content);

    if (new_instance_id.empty())
    {
        meta_data["instance-id"] = YAML::Node{meta_data["instance-id"].as<std::string>() + "_e"};
    }
    else
    {
        meta_data["instance-id"] = YAML::Node{new_instance_id};
    }

    return meta_data;
}

YAML::Node mp::utils::make_cloud_init_network_config(const std::string& default_mac_addr,
                                                     const std::vector<mp::NetworkInterface>& extra_interfaces,
                                                     const std::string& file_content)
{
    YAML::Node network_data = file_content.empty() ? YAML::Node{} : YAML::Load(file_content);
    network_data["version"] = "2";

    const auto default_interface = make_default_interface(default_mac_addr);
    network_data["ethernets"][default_interface.name] = default_interface;

    for (size_t extra_idx = 0; extra_idx < extra_interfaces.size(); ++extra_idx)
    {
        const auto& current = extra_interfaces[extra_idx];

        if (!current.auto_mode)
            continue;

        const auto extra_interface = make_extra_interface(current.mac_address, extra_idx);
        network_data["ethernets"][extra_interface.name] = extra_interface;
    }

    return network_data;
}

YAML::Node mp::utils::add_extra_interface_to_network_config(const std::string& default_mac_addr,
                                                            const NetworkInterface& extra_interface,
                                                            const std::string& network_config_file_content)
{

    if (!extra_interface.auto_mode)
    {
        return network_config_file_content.empty() ? YAML::Node{} : YAML::Load(network_config_file_content);
    }

    if (network_config_file_content.empty()) // for backward compatibility with absent default interface
    {
        YAML::Node network_data{};
        network_data["version"] = "2";

        const auto default_ = make_default_interface(default_mac_addr);
        network_data["ethernets"][default_.name] = default_;

        const auto extra = make_extra_interface(extra_interface.mac_address);
        network_data["ethernets"][extra.name] = extra;

        return network_data;
    }

    YAML::Node network_data = YAML::Load(network_config_file_content);

    // Iterate over possible extra interface names and find a vacant one.
    for (int current_index = 0;; current_index++)
    {
        const auto extra = make_extra_interface(extra_interface.mac_address, current_index);
        if (!network_data["ethernets"][extra.name].IsDefined())
        {
            network_data["ethernets"][extra.name] = extra;
            return network_data;
        }
    }

    throw std::logic_error{"Code execution reached an unreachable path."};
}
