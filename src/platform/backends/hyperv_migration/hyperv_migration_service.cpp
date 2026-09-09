/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hyperv_migration_service.h"

#include <fmt/format.h>

#include <algorithm>

std::vector<multipass::NetworkInterface> multipass::hyperv::translate_extra_interfaces(
    const std::vector<NetworkInterface>& source_interfaces,
    const std::vector<NetworkInterfaceInfo>& available_networks)
{
    const auto find_network =
        [&available_networks](const std::string& id) -> const NetworkInterfaceInfo* {
        const auto it = std::ranges::find(available_networks, id, &NetworkInterfaceInfo::id);
        return it == available_networks.cend() ? nullptr : &*it;
    };

    std::vector<NetworkInterface> translated;
    translated.reserve(source_interfaces.size());

    // Order is significant: the migrated instance must expose its NICs in the same order as
    // the source, so we resolve strictly in source order.
    for (const auto& iface : source_interfaces)
    {
        const auto* const network = find_network(iface.id);
        if (!network)
            throw InstanceMigrationError{
                fmt::format("Cannot migrate networking: source interface '{}' does not map to "
                            "any known Hyper-V network",
                            iface.id)};

        if (network->links.size() != 1)
            throw InstanceMigrationError{
                fmt::format("Cannot migrate networking: Hyper-V network '{}' does not bridge "
                            "exactly one physical adapter (it bridges {})",
                            iface.id,
                            network->links.size())};

        const auto& adapter_id = network->links.front();
        if (adapter_id.empty())
            throw InstanceMigrationError{
                fmt::format("Cannot migrate networking: Hyper-V network '{}' has an empty "
                            "physical adapter link",
                            iface.id)};

        translated.push_back(
            {.id = adapter_id, .mac_address = iface.mac_address, .auto_mode = iface.auto_mode});
    }

    return translated;
}
