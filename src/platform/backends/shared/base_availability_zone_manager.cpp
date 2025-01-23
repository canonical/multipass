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

#include "multipass/base_availability_zone_manager.h"
#include "multipass/base_availability_zone.h"
#include "multipass/exceptions/availability_zone_exceptions.h"
#include "multipass/file_ops.h"
#include "multipass/logging/log.h"

#include <fmt/format.h>

#include <QJsonDocument>

#include <set>

namespace multipass
{

namespace mpl = logging;

constexpr auto category = "az-manager";
constexpr auto automatic_zone_key = "automatic_zone";

BaseAvailabilityZoneManager::data BaseAvailabilityZoneManager::make(const fs::path& data_dir)
{
    std::error_code err;
    const auto zones_directory = data_dir / "zones";

    mpl::log(mpl::Level::info, category, "creating AZ manager");

    const auto read_zone_names = [&] {
        std::set<std::string> default_zones{"zone1", "zone2", "zone3"};

        const auto zones_dir_iter = MP_FILEOPS.dir_iterator(zones_directory, err);
        if (err.value() == static_cast<int>(std::errc::no_such_file_or_directory))
        {
            mpl::log(mpl::Level::info,
                     category,
                     fmt::format("{:?} is missing, attempting to create it", zones_directory.string()));
            MP_FILEOPS.create_directory(zones_directory, err);
            if (err)
            {
                throw AvailabilityZoneManagerDeserializationError{
                    "failed to create {:?}: {}",
                    zones_directory.string(),
                    err.message(),
                };
            }

            mpl::log(mpl::Level::info, category, "using default zones");
            return default_zones;
        }

        if (err)
        {
            throw AvailabilityZoneManagerDeserializationError{
                "failed to access {:?}: {}",
                zones_directory.string(),
                err.message(),
            };
        }

        std::set<std::string> zone_names{};
        while (zones_dir_iter->hasNext())
        {
            const auto& entry = zones_dir_iter->next();
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".json")
                continue;

            mpl::log(mpl::Level::info, category, fmt::format("found AZ file {:?}", entry.path().string()));
            zone_names.insert(entry.path().stem());
        }
        if (zone_names.empty())
        {
            mpl::log(mpl::Level::info, category, "no zones found, using defaults");
            return default_zones;
        }
        return zone_names;
    };

    const auto zone_names = read_zone_names();
    if (zone_names.empty())
    {
        throw AvailabilityZoneManagerDeserializationError{"no zone names retrieved"};
    }

    const auto create_zones = [&] {
        std::map<std::string, AvailabilityZone::UPtr> zones;
        for (const auto& name : zone_names)
            zones[name] = std::make_unique<BaseAvailabilityZone>(name, zones_directory);
        return zones;
    };

    const auto file_path{data_dir / "az_manager.json"};
    const auto file_type = MP_FILEOPS.status(file_path, err).type();
    if (err && file_type != fs::file_type::not_found)
    {
        throw AvailabilityZoneManagerDeserializationError{
            "AZ manager file {:?} is not accessible: {}.",
            file_path.string(),
            err.message(),
        };
    }

    if (file_type == fs::file_type::not_found)
    {
        mpl::log(mpl::Level::info,
                 category,
                 fmt::format("AZ manager file {:?} not found, using defaults", file_path.string()));
        return {
            .file_path = file_path,
            .zones = create_zones(),
            .automatic_zone = *zone_names.begin(),
        };
    }

    if (file_type != fs::file_type::regular)
    {
        throw AvailabilityZoneManagerDeserializationError{
            "AZ manager file {:?} is not a regular file.",
            file_path.string(),
        };
    }

    mpl::log(mpl::Level::info, category, fmt::format("reading AZ manager from file {:?}", file_path.string()));
    const auto file = MP_FILEOPS.open_read(file_path);
    if (file->fail())
    {
        throw AvailabilityZoneManagerDeserializationError{
            "failed to read AZ manager file {:?}: {}",
            file_path.string(),
            std::strerror(errno),
        };
    }

    const std::string data{std::istreambuf_iterator{*file}, {}};
    const auto qdata = QString::fromStdString(data).toUtf8();
    const auto json = QJsonDocument::fromJson(qdata).object();

    const auto deserialize_automatic_zone = [&] {
        auto json_automatic_zone = json[automatic_zone_key].toString().toStdString();
        if (zone_names.find(json_automatic_zone) != zone_names.end())
            return json_automatic_zone;

        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("automatic zone {:?} not known, using default", json_automatic_zone));
        return *zone_names.begin();
    };

    return {
        .file_path = file_path,
        .zones = create_zones(),
        .automatic_zone = deserialize_automatic_zone(),
    };
}

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir) : m{make(data_dir)}
{
    serialize();
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    if (const auto it = m.zones.find(name); it != m.zones.end())
        return *it->second;
    throw AvailabilityZoneNotFound{name};
}

std::string BaseAvailabilityZoneManager::get_automatic_zone_name()
{
    const std::unique_lock lock{m.mutex};
    // round-robin until we get an available zone
    const auto start = m.zones.find(m.automatic_zone);
    auto current = start;
    do
    {
        if (const auto& [zone_name, zone] = *current; zone->is_available())
        {
            const auto result = zone_name;
            if (++current == m.zones.end())
                current = m.zones.begin();
            m.automatic_zone = current->first;
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
    for (auto& [_, zone] : m.zones)
        zone_list.emplace_back(*zone);
    return zone_list;
}
std::string BaseAvailabilityZoneManager::get_default_zone_name() const
{
    return m.zones.begin()->first;
}

void BaseAvailabilityZoneManager::serialize() const
{
    const std::unique_lock lock{m.mutex};
    mpl::log(mpl::Level::info, category, fmt::format("writing AZ manager to file {:?}", m.file_path.string()));

    const QJsonObject json{
        {automatic_zone_key, QString::fromStdString(m.automatic_zone)},
    };
    const auto json_bytes = QJsonDocument{json}.toJson();

    if (MP_FILEOPS.open_write(m.file_path)->write(json_bytes.data(), json_bytes.size()).fail())
    {
        throw AvailabilityZoneManagerSerializationError{
            "failed to write to AZ manager file {:?}: {}",
            m.file_path.string(),
            std::strerror(errno),
        };
    }
}
} // namespace multipass
