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

#include <initializer_list>
#include <memory>
#include <vector>

#include "availability_zone_manager.h"
#include "stub_availability_zone.h"

namespace multipass
{
// A minimal AvailabilityZoneManager for tests and backends that don't support availability zones
// (e.g. legacy VirtualBox and Hyper-V). It provides one or more always-available
// StubAvailabilityZones that can be used in place of a real AZ.
class StubAvailabilityZoneManager final : public AvailabilityZoneManager
{
public:
    StubAvailabilityZoneManager();
    StubAvailabilityZoneManager(const std::initializer_list<Subnet> subnets);

    AvailabilityZone& get_zone(const std::string& name) override;
    const AvailabilityZone& get_zone(const std::string& name) const override;
    std::string get_automatic_zone_name() override;
    std::vector<std::reference_wrapper<const AvailabilityZone>> get_zones() const override;
    std::string get_default_zone_name() const override;

private:
    std::vector<std::unique_ptr<StubAvailabilityZone>> zones;
};
} // namespace multipass

#endif // MULTIPASS_STUB_AVAILABILITY_ZONE_MANAGER_H
