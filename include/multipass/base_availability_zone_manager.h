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

#ifndef MULTIPASS_BASE_AVAILABILITY_ZONE_MANAGER_H
#define MULTIPASS_BASE_AVAILABILITY_ZONE_MANAGER_H

#include "availability_zone_manager.h"

#include <multipass/constants.h>

#include <array>
#include <filesystem>
#include <mutex>
#include <shared_mutex>

namespace multipass
{
class BaseAvailabilityZoneManager final : public AvailabilityZoneManager
{
public:
    explicit BaseAvailabilityZoneManager(const std::filesystem::path& data_dir);

    AvailabilityZone& get_zone(const std::string& name) override;
    std::vector<std::reference_wrapper<const AvailabilityZone>> get_zones() override;
    std::string get_automatic_zone_name() override;
    std::string get_default_zone_name() const override;

private:
    void serialize() const;

    class ZoneCollection
    {
    public:
        static constexpr size_t size = default_zone_names.size();
        using ZoneArray = std::array<AvailabilityZone::UPtr, size>;
        static_assert(size > 0);

        const ZoneArray zones{};

        ZoneCollection(ZoneArray&& zones, std::string last_used);
        [[nodiscard]] std::string next_available();
        [[nodiscard]] std::string last_used() const;

    private:
        ZoneArray::const_iterator automatic_zone;
        mutable std::shared_mutex mutex{};
    };

    mutable std::recursive_mutex mutex;
    const std::filesystem::path file_path;
    ZoneCollection zone_collection;

    [[nodiscard]] const ZoneCollection::ZoneArray& zones() const;

    static ZoneCollection read_from_file(const std::filesystem::path& file_path,
                                         const std::filesystem::path& zones_directory);
};
} // namespace multipass

#endif // MULTIPASS_BASE_AVAILABILITY_ZONE_MANAGER_H
