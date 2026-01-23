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
#include "stub_availability_zone.h"

namespace multipass
{
namespace test
{
struct StubAvailabilityZoneManager final : public AvailabilityZoneManager
{
    StubAvailabilityZoneManager()
    {
    }

    AvailabilityZone& get_zone(const std::string& name) override
    {
        return zone;
    }
    std::string get_automatic_zone_name() override
    {
        return "zone1";
    }
    std::vector<std::reference_wrapper<const AvailabilityZone>> get_zones() override
    {
        return {zone};
    }

    std::string get_default_zone_name() const override
    {
        return "zone1";
    }

private:
    StubAvailabilityZone zone{};
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_AVAILABILITY_ZONE_MANAGER_H
