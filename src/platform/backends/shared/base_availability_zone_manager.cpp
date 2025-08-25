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

#include <multipass/base_availability_zone.h>
#include <multipass/base_availability_zone_manager.h>
#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <QJsonDocument>

namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "az-manager";
constexpr auto az_file = "az-manager.json";
constexpr auto zones_directory_name = "zones";
constexpr auto automatic_zone_key = "automatic_zone";

[[nodiscard]] auto create_default_zones(const multipass::fs::path& zones_directory)
{
    using namespace multipass;

    std::array<AvailabilityZone::UPtr, default_zone_names.size()> zones{};
    std::transform(default_zone_names.begin(),
                   default_zone_names.end(),
                   zones.begin(),
                   [&](const auto& zone_name) {
                       return std::make_unique<BaseAvailabilityZone>(zone_name, zones_directory);
                   });
    return zones;
};

} // namespace

namespace multipass
{

BaseAvailabilityZoneManager::data BaseAvailabilityZoneManager::read_from_file(
    const std::filesystem::path& file_path,
    const std::filesystem::path& zones_directory)
{
    mpl::debug(category, "reading AZ manager from file '{}'", file_path);
    return {
        // TODO remove these comments in C++20
        /*.file_path = */ file_path,
        /*.zone_collection = */
        {create_default_zones(zones_directory), "zone1"},
    };
}

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir)
    : m{read_from_file(data_dir / az_file, data_dir / zones_directory_name)}
{
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    for (const auto& zone : zones())
    {
        if (zone->get_name() == name)
            return *zone;
    }
    throw AvailabilityZoneNotFound{name};
}


std::vector<std::reference_wrapper<const AvailabilityZone>> BaseAvailabilityZoneManager::get_zones()
{
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    zone_list.reserve(zones().size());
    for (auto& zone : zones())
        zone_list.emplace_back(*zone);
    return zone_list;
}

std::string BaseAvailabilityZoneManager::get_default_zone_name() const
{
    return (*zones().begin())->get_name();
}


const BaseAvailabilityZoneManager::ZoneCollection::ZoneArray&
BaseAvailabilityZoneManager::zones() const
{
    return m.zone_collection.zones;
}

BaseAvailabilityZoneManager::ZoneCollection::ZoneCollection(
    std::array<AvailabilityZone::UPtr, default_zone_names.size()>&& _zones,
    std::string)
    : zones{std::move(_zones)}
{
}


} // namespace multipass
