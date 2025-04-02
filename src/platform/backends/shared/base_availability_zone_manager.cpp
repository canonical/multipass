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

namespace multipass
{
namespace mpl = logging;

constexpr auto category = "az-manager";
constexpr auto az_file = "az_manager.json";
constexpr auto zones_directory_name = "zones";
constexpr auto automatic_zone_key = "automatic_zone";

auto create_default_zones(const fs::path& zones_directory)
{
    std::array<AvailabilityZone::UPtr, default_zone_names.size()> zones{};
    std::transform(default_zone_names.begin(), default_zone_names.end(), zones.begin(), [&](const auto& zone_name) {
        return std::make_unique<BaseAvailabilityZone>(zone_name, zones_directory);
    });
    return zones;
};

BaseAvailabilityZoneManager::data BaseAvailabilityZoneManager::read_from_file(const fs::path& file_path,
                                                                              const fs::path& zones_directory)
{
    mpl::trace(category, "reading AZ manager from file '{}'", file_path);
    const auto json = MP_JSONUTILS.read_object_from_file(file_path);

    const auto deserialize_automatic_zone = [&] {
        auto json_automatic_zone = json[automatic_zone_key].toString().toStdString();
        if (std::find(default_zone_names.begin(), default_zone_names.end(), json_automatic_zone) !=
            default_zone_names.end())
            return json_automatic_zone;

        mpl::debug(category, "automatic zone '{}' not known, using default", json_automatic_zone);
        return std::string{*default_zone_names.begin()};
    };

    return {
        // TODO remove these comments in C++20
        /*.file_path = */ file_path,
        /*.zones = */ create_default_zones(zones_directory),
        /*.automatic_zone = */ deserialize_automatic_zone(),
    };
}

BaseAvailabilityZoneManager::data BaseAvailabilityZoneManager::make(const fs::path& data_dir)
{
    const auto file_path = data_dir / az_file;
    const auto zones_directory = data_dir / zones_directory_name;

    mpl::trace(category, "creating AZ manager");

    if (std::error_code err{}; MP_FILEOPS.exists(file_path, err))
        return read_from_file(file_path, zones_directory);
    else if (err)
        throw FormattedExceptionBase{"AZ manager file '{}' is not accessible: {}", file_path, err.message()};

    mpl::debug(category, "AZ manager file '{}' not found, using defaults", file_path);
    return {
        // TODO remove these comments in C++20
        /*.file_path = */ file_path,
        /*.zones = */ create_default_zones(zones_directory),
        /*.automatic_zone = */ *default_zone_names.begin(),
    };
}

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir) : m{make(data_dir)}
{
    serialize();
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    for (const auto& zone : m.zones)
    {
        if (zone->get_name() == name)
            return *zone;
    }
    throw AvailabilityZoneNotFound{name};
}

std::string BaseAvailabilityZoneManager::get_automatic_zone_name()
{
    const std::unique_lock lock{m.mutex};
    // round-robin until we get an available zone
    const auto start = std::find_if(m.zones.begin(), m.zones.end(), [&](const auto& zone) {
        return zone->get_name() == m.automatic_zone;
    });
    auto current = start;
    do
    {
        if (current->get()->is_available())
        {
            const auto result = current->get()->get_name();
            if (++current == m.zones.end())
                current = m.zones.begin();
            m.automatic_zone = current->get()->get_name();
            serialize();
            return result;
        }

        if (++current == m.zones.end())
            current = m.zones.begin();
    } while (current != start);

    throw NoAvailabilityZoneAvailable{};
}

std::vector<std::reference_wrapper<const AvailabilityZone>> BaseAvailabilityZoneManager::get_zones()
{
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    zone_list.reserve(m.zones.size());
    for (auto& zone : m.zones)
        zone_list.emplace_back(*zone);
    return zone_list;
}

std::string BaseAvailabilityZoneManager::get_default_zone_name() const
{
    return m.zones.begin()->get()->get_name();
}

void BaseAvailabilityZoneManager::serialize() const
{
    mpl::trace(category, "writing AZ manager to file '{}'", m.file_path);
    const std::unique_lock lock{m.mutex};

    const QJsonObject json{
        {automatic_zone_key, QString::fromStdString(m.automatic_zone)},
    };

    MP_JSONUTILS.write_json(json, QString::fromStdString(m.file_path.u8string()));
}
} // namespace multipass
