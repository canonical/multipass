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

#include <filesystem>
#include <map>
#include <mutex>

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

    // we store all the data in one struct so that it can be created from one function call in the initializer list
    struct data
    {
        const std::filesystem::path file_path{};
        const std::map<std::string, AvailabilityZone::UPtr> zones{};

        mutable std::recursive_mutex mutex{};
        std::string automatic_zone{};
    } m;

    static data make(const std::filesystem::path& data_dir);
};
} // namespace multipass

#endif // MULTIPASS_BASE_AVAILABILITY_ZONE_MANAGER_H
