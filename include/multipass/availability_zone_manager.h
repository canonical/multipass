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

#ifndef MULTIPASS_AVAILABILITY_ZONE_MANAGER_H
#define MULTIPASS_AVAILABILITY_ZONE_MANAGER_H

#include "availability_zone.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace multipass
{
class AvailabilityZoneManager : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<AvailabilityZoneManager>;
    using ShPtr = std::shared_ptr<AvailabilityZoneManager>;

    using Zones = std::vector<std::reference_wrapper<const AvailabilityZone>>;

    virtual ~AvailabilityZoneManager() = default;

    virtual AvailabilityZone& get_zone(const std::string& name) = 0;
    virtual Zones get_zones() = 0;
    // this returns a computed zone name, using an algorithm e.g. round-robin
    // not to be confused with [get_default_zone_name]
    virtual std::string get_automatic_zone_name() = 0;
    // this always returns the same zone name, to be given to VMs that were not assigned to a zone
    // in the past not to be confused with [get_automatic_zone]
    virtual std::string get_default_zone_name() const = 0;
};
} // namespace multipass

#endif // MULTIPASS_AVAILABILITY_ZONE_MANAGER_H
