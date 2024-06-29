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
#include <cassert>

#include <multipass/format.h>
#include <multipass/network_interface.h>
#include <multipass/yaml_node_utils.h>

namespace mp = multipass;

namespace
{
YAML::Node create_extra_interface_node(const std::string& extra_interface_name,
                                       const std::string& extra_interface_mac_address)
{
    YAML::Node extra_interface_data{};
    extra_interface_data["match"]["macaddress"] = extra_interface_mac_address;
    extra_interface_data["dhcp4"] = true;
    // We make the default gateway associated with the first interface.
    extra_interface_data["dhcp4-overrides"]["route-metric"] = 200;
    // Make the interface optional, which means that networkd will not wait for the device to be configured.
    extra_interface_data["optional"] = true;

    return extra_interface_data;
};
} // namespace

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

    // Generate the cloud-init file only if there is at least one extra interface needing auto configuration.
    if (std::find_if(extra_interfaces.begin(), extra_interfaces.end(), [](const auto& iface) {
            return iface.auto_mode;
        }) != extra_interfaces.end())
    {
        network_data["version"] = "2";

        std::string name = "default";
        network_data["ethernets"][name]["match"]["macaddress"] = default_mac_addr;
        network_data["ethernets"][name]["dhcp4"] = true;

        for (size_t i = 0; i < extra_interfaces.size(); ++i)
        {
            if (extra_interfaces[i].auto_mode)
            {
                name = "extra" + std::to_string(i);
                network_data["ethernets"][name] = create_extra_interface_node(name, extra_interfaces[i].mac_address);
            }
        }
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

    if (network_config_file_content.empty())
    {
        YAML::Node network_data{};
        network_data["version"] = "2";

        const std::string default_network_name = "default";
        network_data["ethernets"][default_network_name]["match"]["macaddress"] = default_mac_addr;
        network_data["ethernets"][default_network_name]["dhcp4"] = true;

        const std::string extra_interface_name = "extra0";
        network_data["ethernets"][extra_interface_name] =
            create_extra_interface_node(extra_interface_name, extra_interface.mac_address);

        return network_data;
    }

    YAML::Node network_data = YAML::Load(network_config_file_content);

    int i = 0;
    while (true)
    {
        const std::string extra_interface_name = "extra" + std::to_string(i);
        if (!network_data["ethernets"][extra_interface_name].IsDefined())
        {
            // append the new network interface
            network_data["ethernets"][extra_interface_name] =
                create_extra_interface_node(extra_interface_name, extra_interface.mac_address);

            return network_data;
        }

        ++i;
    }
}
