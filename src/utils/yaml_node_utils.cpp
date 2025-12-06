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

namespace
{

constexpr std::size_t kDefaultInterfaceIndex = 0;
constexpr std::size_t kExtraInterfaceIndexStart = kDefaultInterfaceIndex + 1;
constexpr std::string_view kInterfaceNamePattern = "eth{}";

struct interface_details
{
    std::string name;
    std::string mac_addr;
    bool optional{false};
    std::optional<int> route_metric{std::nullopt};

    interface_details(const std::string& mac_addr,
                      std::size_t index = kDefaultInterfaceIndex,
                      bool optional = false)
        : name(fmt::format(kInterfaceNamePattern, index)),
          mac_addr(mac_addr),
          optional(optional),
          route_metric(optional ? std::make_optional(200) : std::nullopt)
    {
    }
};

} // namespace

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
        // Make the interface optional, which means that networkd will not wait for the device to be
        // configured.

        if (iface.optional)
        {
            node["optional"] = true;
        }
        node["set-name"] = iface.name;
        return node;
    }
};
} // namespace YAML

std::string mp::utils::emit_yaml(const YAML::Node& node)
{
    YAML::Emitter emitter;
    emitter.SetIndent(2);

    // Helper function to handle nodes recursively
    std::function<void(const YAML::Node&)> emit_node;
    emit_node = [&emitter, &emit_node](const YAML::Node& n) {
        switch (n.Type())
        {
        case YAML::NodeType::Map:
        {
            emitter << YAML::BeginMap;
            for (const auto& kv : n)
            {
                emitter << YAML::Key;
                emit_node(kv.first);
                emitter << YAML::Value;
                emit_node(kv.second);
            }
            emitter << YAML::EndMap;
            break;
        }
        case YAML::NodeType::Sequence:
        {
            emitter << YAML::BeginSeq;
            for (const auto& v : n)
            {
                emit_node(v);
            }
            emitter << YAML::EndSeq;
            break;
        }
        default:
        {
            // Special handling for strings that look like octal numbers (e.g. "0755")
            // or contain colons
            if (n.IsScalar())
            {
                const std::string value = n.Scalar();
                // If the node is a scalar string that contains a colon, emit it explicitly as a
                // double-quoted string to prevent YAML from interpreting it as e.g. a timestamp.
                if (value.find(':') != std::string::npos)
                {
                    emitter << YAML::DoubleQuoted << value;
                    break;
                }
                // If the node is a scalar string that looks like an octal number, emit
                // it explicitly as a string to prevent YAML from interpreting it as an octal value.
                if (value.length() >= 2 && value[0] == '0' &&
                    std::all_of(value.begin() + 1, value.end(), ::isdigit))
                {
                    emitter << YAML::DoubleQuoted << value;
                    break;
                }
            }
            emitter << n;
        }
        }
    };

    emit_node(node);

    if (!emitter.good())
        throw std::runtime_error{fmt::format("Failed to emit YAML: {}", emitter.GetLastError())};

    emitter << YAML::Newline;
    return emitter.c_str();
}

std::string mp::utils::emit_cloud_config(const YAML::Node& node)
{
    return fmt::format("#cloud-config\n{}", emit_yaml(node));
}

YAML::Node mp::utils::make_cloud_init_meta_config(const std::string& name,
                                                  const std::string& file_content)
{
    YAML::Node meta_data = file_content.empty() ? YAML::Node{} : YAML::Load(file_content);

    if (!file_content.empty())
    {
        const std::string old_hostname = meta_data["local-hostname"].as<std::string>();
        std::string old_instance_id = meta_data["instance-id"].as<std::string>();

        // The assumption here is that the instance_id is the hostname optionally appended _e
        // sequence
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

YAML::Node mp::utils::make_cloud_init_network_config(
    const std::string& default_mac_addr,
    const std::vector<mp::NetworkInterface>& extra_interfaces,
    const std::string& file_content)
{
    YAML::Node network_data = file_content.empty() ? YAML::Node{} : YAML::Load(file_content);
    network_data["version"] = "2";

    const auto default_interface = interface_details{default_mac_addr};
    network_data["ethernets"][default_interface.name] = default_interface;

    // TODO@C++23: https://en.cppreference.com/w/cpp/ranges/enumerate_view.html
    for (auto extra_idx = kExtraInterfaceIndexStart; const auto& extra : extra_interfaces)
    {
        if (!extra.auto_mode)
            continue;
        const auto extra_interface = interface_details{/*mac_addr=*/extra.mac_address,
                                                       /*interface_idx=*/extra_idx,
                                                       /*optional=*/true};
        network_data["ethernets"][extra_interface.name] = extra_interface;
        ++extra_idx;
    }

    return network_data;
}

YAML::Node mp::utils::add_extra_interface_to_network_config(
    const std::string& default_mac_addr,
    const NetworkInterface& extra_interface,
    const std::string& network_config_file_content)
{

    if (!extra_interface.auto_mode)
    {
        return network_config_file_content.empty() ? YAML::Node{}
                                                   : YAML::Load(network_config_file_content);
    }

    if (network_config_file_content
            .empty()) // for backward compatibility with absent default interface
    {
        YAML::Node network_data{};
        network_data["version"] = "2";

        const interface_details default_interface{default_mac_addr};
        network_data["ethernets"][default_interface.name] = default_interface;
        const interface_details extra{/*mac_addr=*/extra_interface.mac_address,
                                      /*index=*/kExtraInterfaceIndexStart,
                                      /*optional=*/true};
        network_data["ethernets"][extra.name] = extra;

        return network_data;
    }

    YAML::Node network_data = YAML::Load(network_config_file_content);

    // Iterate over possible extra interface names and find a vacant one.
    for (std::size_t current_index = kExtraInterfaceIndexStart;; current_index++)
    {
        const interface_details extra{/*mac_addr=*/extra_interface.mac_address,
                                      /*index=*/current_index,
                                      /*optional=*/true};
        if (!network_data["ethernets"][extra.name].IsDefined())
        {
            network_data["ethernets"][extra.name] = extra;
            return network_data;
        }
    }

    throw std::logic_error{"Code execution reached an unreachable path."};
}
