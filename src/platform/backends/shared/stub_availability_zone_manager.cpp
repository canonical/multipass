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

#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/stub_availability_zone_manager.h>

namespace multipass
{

StubAvailabilityZoneManager::StubAvailabilityZoneManager()
{
    zones.push_back(std::make_unique<StubAvailabilityZone>("zone1"));
}

StubAvailabilityZoneManager::StubAvailabilityZoneManager(
    const std::initializer_list<Subnet> subnets)
{
    int i = 1;
    for (const auto& subnet : subnets)
        zones.push_back(std::make_unique<StubAvailabilityZone>(fmt::format("zone{}", i++), subnet));
}

AvailabilityZone& StubAvailabilityZoneManager::get_zone(const std::string& name)
{
    return const_cast<AvailabilityZone&>(std::as_const(*this).get_zone(name));
}

const AvailabilityZone& StubAvailabilityZoneManager::get_zone(const std::string& name) const
{
    for (const auto& zone : zones)
    {
        if (zone->get_name() == name)
            return *zone;
    }
    throw AvailabilityZoneNotFound{name};
}

std::string StubAvailabilityZoneManager::get_automatic_zone_name()
{
    return zones[0]->get_name();
}

std::vector<std::reference_wrapper<const AvailabilityZone>>
StubAvailabilityZoneManager::get_zones() const
{
    std::vector<std::reference_wrapper<const AvailabilityZone>> zone_list;
    for (auto& zone : zones)
        zone_list.push_back(*zone);
    return zone_list;
}

std::string StubAvailabilityZoneManager::get_default_zone_name() const
{
    return zones[0]->get_name();
}

} // namespace multipass
