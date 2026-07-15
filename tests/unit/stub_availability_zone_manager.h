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

#ifndef MULTIPASS_STUB_AVAILABILITY_ZONE_MANAGER_H
#define MULTIPASS_STUB_AVAILABILITY_ZONE_MANAGER_H

#include "multipass/availability_zone_manager.h"
#include "multipass/exceptions/availability_zone_exceptions.h"
#include "stub_availability_zone.h"

#include <fmt/format.h>

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

namespace multipass
{
namespace test
{
struct StubAvailabilityZoneManager final : public AvailabilityZoneManager
{
    StubAvailabilityZoneManager()
    {
        zones.push_back(std::make_unique<StubAvailabilityZone>());
    }

    StubAvailabilityZoneManager(const std::initializer_list<Subnet> subnets)
    {
        int i = 1;
        for (const auto& subnet : subnets)
            zones.push_back(
                std::make_unique<StubAvailabilityZone>(fmt::format("zone{}", i++), subnet));
    }

    AvailabilityZone& get_zone(const std::string& name) override
    {
        return const_cast<AvailabilityZone&>(std::as_const(*this).get_zone(name));
    }
    const AvailabilityZone& get_zone(const std::string& name) const override
    {
        for (const auto& zone : zones)
        {
            if (zone->get_name() == name)
                return *zone;
        }
        throw AvailabilityZoneNotFound{name};
    }
    std::string get_automatic_zone_name() override
    {
        return zones[0]->get_name();
    }
    std::vector<std::reference_wrapper<const AvailabilityZone>> get_zones() const override
    {
        std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
        for (auto& zone : zones)
            zone_list.push_back(*zone);
        return zone_list;
    }

    std::string get_default_zone_name() const override
    {
        return zones[0]->get_name();
    }

private:
    std::vector<std::unique_ptr<StubAvailabilityZone>> zones;
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_AVAILABILITY_ZONE_MANAGER_H
