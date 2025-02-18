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

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir)
    : file_path{data_dir / "az_manager.json"}
{

    std::error_code err;
    const auto zones_directory = data_dir / "zones";

    mpl::log(mpl::Level::info, category, "creating AZ manager");

    const auto get_zone_names = [&] {
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

    // Get zone names first and set default automatic zone early
    const auto zone_names = get_zone_names();
    automatic_zone = *zone_names.begin();

    // BUGFIX: Previously, zone objects were created before handling the manager file,
    // and an early return in the file_type::not_found case prevented zone creation.
    // Now we handle all file operations first, then create zone objects at the end
    // to ensure they are always created regardless of file operations.

    // Check manager file first before creating zones
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
        serialize();
    }
    else if (file_type != fs::file_type::regular)
    {
        throw AvailabilityZoneManagerDeserializationError{
            "AZ manager file {:?} is not a regular file.",
            file_path.string(),
        };
    }
    else
    {
        mpl::log(mpl::Level::info, category, fmt::format("reading AZ manager from file {:?}", file_path.string()));
        const auto file = MP_FILEOPS.open_read(file_path);
        if (file->fail())
        {
            throw AvailabilityZoneManagerDeserializationError{
                "failed to open AZ manager file {:?} for reading: {}",
                file_path.string(),
                std::strerror(errno),
            };
        }

        const std::string data{std::istreambuf_iterator{*file}, {}};
        const auto qdata = QString::fromStdString(data).toUtf8();
        const auto json = QJsonDocument::fromJson(qdata).object();

        if (const auto json_automatic_zone = json[automatic_zone_key].toString().toStdString();
            zone_names.find(json_automatic_zone) == zone_names.end())
        {
            mpl::log(mpl::Level::warning,
                     category,
                     fmt::format("automatic zone {:?} not known, using default", json_automatic_zone));
        }
        else
        {
            automatic_zone = json_automatic_zone;
        }
    }

    // Create zone objects after all file operations are complete
    // This ensures zones are always created regardless of file operations
    for (const auto& name : zone_names)
    {
        zones[name] = std::make_unique<BaseAvailabilityZone>(name, zones_directory);
    }
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    if (const auto it = zones.find(name); it != zones.end())
        return *it->second;
    throw AvailabilityZoneNotFound{name};
}

std::string BaseAvailabilityZoneManager::get_automatic_zone_name()
{
    const std::unique_lock lock{mutex};
    // round-robin until we get an available zone
    const auto start = zones.find(automatic_zone);
    auto current = start;
    do
    {
        if (current->second->is_available())
        {
            const auto result = current->first;
            if (++current == zones.end())
                current = zones.begin();
            automatic_zone = current->first;
            serialize();
            return result;
        }

        if (++current == zones.end())
            current = zones.begin();
    } while (current != start);

    throw NoAvailabilityZoneAvailable{};
}

std::vector<std::reference_wrapper<const AvailabilityZone>> BaseAvailabilityZoneManager::get_zones()
{
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    zone_list.reserve(zones.size());
    for (auto& [_, zone] : zones)
        zone_list.emplace_back(*zone);
    return zone_list;
}
std::string BaseAvailabilityZoneManager::get_default_zone_name() const
{
    return zones.begin()->first;
}

void BaseAvailabilityZoneManager::serialize() const
{
    const std::unique_lock lock{mutex};
    mpl::log(mpl::Level::info, category, fmt::format("writing AZ manager to file {:?}", file_path.string()));

    const QJsonObject json{
        {automatic_zone_key, QString::fromStdString(automatic_zone)},
    };
    const auto json_bytes = QJsonDocument{json}.toJson();

    const auto file = MP_FILEOPS.open_write(file_path);
    if (file->fail())
    {
        throw AvailabilityZoneManagerSerializationError{
            "failed to open AZ manager file {:?} for writing: {}",
            file_path.string(),
            std::strerror(errno),
        };
    }

    if (file->write(json_bytes.data(), json_bytes.size()).fail())
    {
        throw AvailabilityZoneManagerSerializationError{
            "failed to write to AZ manager file {:?}: {}",
            file_path.string(),
            std::strerror(errno),
        };
    }
}
} // namespace multipass
