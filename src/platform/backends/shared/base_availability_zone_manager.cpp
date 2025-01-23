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
#include "multipass/file_ops.h"
#include "multipass/logging/log.h"

#include <fmt/format.h>

#include <QJsonDocument>

#include <set>

namespace multipass
{

namespace mpl = logging;

constexpr auto category = "az-manager";

BaseAvailabilityZoneManager::BaseAvailabilityZoneManager(const fs::path& data_dir)
    : file_path{data_dir / "az_manager.json"}
{

    std::error_code err;
    const auto zones_directory = data_dir / "zones";

    mpl::log(mpl::Level::info, category, "creating AZ manager");

    const auto get_zone_names = [&] {
        const std::set<std::string> default_zones{"zone1", "zone2", "zone3"};
        const auto zones_dir_iter = MP_FILEOPS.dir_iterator(zones_directory, err);
        if (err.value() == static_cast<int>(std::errc::no_such_file_or_directory))
        {
            mpl::log(mpl::Level::info,
                     category,
                     fmt::format("{:?} is missing, attempting to create it", zones_directory.string()));
            MP_FILEOPS.create_directory(zones_directory, err);
            if (err)
                throw std::runtime_error{
                    fmt::format("failed to create {:?}: {}", zones_directory.string(), err.message())};
            mpl::log(mpl::Level::info, category, "using default zones");
            return default_zones;
        }

        if (err)
            throw std::runtime_error{fmt::format("failed to access {:?}: {}", zones_directory.string(), err.message())};

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

    const auto zone_names = get_zone_names();

    const auto file_type = MP_FILEOPS.status(file_path, err).type();
    if (err && file_type != fs::file_type::not_found)
    {
        const auto message =
            fmt::format("AZ manager file {:?} is not accessible: {}.", file_path.string(), err.message());
        throw std::runtime_error(message);
    }

    if (file_type == fs::file_type::not_found)
    {
        mpl::log(mpl::Level::info,
                 category,
                 fmt::format("AZ manager file {:?} not found, using defaults", file_path.string()));
        automatic_zone = *zone_names.begin();
        serialize();
        return;
    }

    if (file_type != fs::file_type::regular)
        throw std::runtime_error(fmt::format("AZ manager file {:?} is not a regular file.", file_path.string()));

    mpl::log(mpl::Level::info, category, fmt::format("reading AZ manager from file {:?}", file_path.string()));
    const auto file = MP_FILEOPS.open_read(file_path);
    if (file->fail())
    {
        const auto message = fmt::format("failed to open AZ manager file {:?} for reading: {}",
                                         file_path.string(),
                                         std::strerror(errno));
        throw std::runtime_error{message};
    }
    const std::string data{std::istreambuf_iterator{*file}, {}};
    const auto qdata = QString::fromStdString(data).toUtf8();
    const auto json = QJsonDocument::fromJson(qdata).object();

    const auto json_automatic_zone = json["automatic_zone"].toString().toStdString();
    if (zone_names.find(json_automatic_zone) == zone_names.end())
    {
        mpl::log(mpl::Level::warning,
                 category,
                 fmt::format("automatic zone {:?} not known, using default", json_automatic_zone));
        automatic_zone = *zone_names.begin();
    }
    else
    {
        automatic_zone = json_automatic_zone;
    }

    serialize();

    for (const auto& name : zone_names)
        zones[name] = std::make_unique<BaseAvailabilityZone>(name, zones_directory);
}

AvailabilityZone& BaseAvailabilityZoneManager::get_zone(const std::string& name)
{
    const std::unique_lock lock{mutex};
    return *zones.at(name);
}

std::string BaseAvailabilityZoneManager::get_automatic_zone_name()
{
    const std::unique_lock lock{mutex};
    // round-robin until we get an available zone
    auto start = zones.find(automatic_zone);
    auto current = start;
    do
    {
        if (current->second->is_available())
        {
            const auto result = current->first;
            ++current;
            if (current == zones.end())
                current = zones.begin();
            automatic_zone = current->first;
            serialize();
            return result;
        }
        ++current;
        if (current == zones.end())
            current = zones.begin();
    } while (current != start);

    throw std::runtime_error("no AZ is available");
}

std::vector<std::reference_wrapper<const AvailabilityZone>> BaseAvailabilityZoneManager::get_zones()
{
    const std::unique_lock lock{mutex};
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    for (auto& [_, zone] : zones)
        zone_list.push_back(*zone);
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
    QJsonObject json{};
    json["automatic_zone"] = QString::fromStdString(automatic_zone);
    const auto json_bytes = QJsonDocument{json}.toJson();
    const auto file = MP_FILEOPS.open_write(file_path);
    if (file->fail())
    {
        throw std::runtime_error{fmt::format("failed to open AZ manager file {:?} for writing: {}",
                                             file_path.string(),
                                             std::strerror(errno))};
    }
    if (file->write(json_bytes.data(), json_bytes.size()).fail())
    {
        throw std::runtime_error{
            fmt::format("failed write to AZ manager file {:?}: {}", file_path.string(), std::strerror(errno))};
    }
}
} // namespace multipass
