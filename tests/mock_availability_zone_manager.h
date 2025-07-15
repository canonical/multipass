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

#ifndef MULTIPASS_MOCK_AVAILABILITY_ZONE_MANAGER_H
#define MULTIPASS_MOCK_AVAILABILITY_ZONE_MANAGER_H

#include "mock_availability_zone.h"

#include <gmock/gmock.h>
#include <multipass/availability_zone_manager.h>

namespace multipass
{
namespace test
{

namespace mp = multipass;

struct MockAvailabilityZoneManager : public mp::AvailabilityZoneManager
{
    MOCK_METHOD(mp::AvailabilityZone&, get_zone, (const std::string&), (override));
    MOCK_METHOD(std::vector<std::reference_wrapper<const mp::AvailabilityZone>>, get_zones, (), (override));
    MOCK_METHOD(std::string, get_automatic_zone_name, (), (override));
    MOCK_METHOD(std::string, get_default_zone_name, (), (const, override));
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_AVAILABILITY_ZONE_MANAGER_H
