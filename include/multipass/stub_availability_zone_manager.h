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

#include "availability_zone_manager.h"
#include "stub_availability_zone.h"

namespace multipass
{
// An AvailabilityZoneManager for backends that do not support the concept of availability zones
// (e.g. legacy VirtualBox and Hyper-V). It hands out a single, always-available
// StubAvailabilityZone regardless of the name requested, so that stale or empty zone names
// persisted by an older build (or coming in over the wire) never cause a lookup failure -- the
// name argument is intentionally ignored rather than validated.
class StubAvailabilityZoneManager final : public AvailabilityZoneManager
{
public:
    AvailabilityZone& get_zone(const std::string& /*name*/) override
    {
        return zone;
    }

    const AvailabilityZone& get_zone(const std::string& /*name*/) const override
    {
        return zone;
    }

    Zones get_zones() const override
    {
        return {zone};
    }

    std::string get_automatic_zone_name() override
    {
        return zone.get_name();
    }

    std::string get_default_zone_name() const override
    {
        return zone.get_name();
    }

private:
    StubAvailabilityZone zone;
};
} // namespace multipass

#endif // MULTIPASS_STUB_AVAILABILITY_ZONE_MANAGER_H
