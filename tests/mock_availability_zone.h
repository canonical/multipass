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

#ifndef MULTIPASS_MOCK_AVAILABILITY_ZONE_H
#define MULTIPASS_MOCK_AVAILABILITY_ZONE_H

#include <gmock/gmock.h>
#include <multipass/availability_zone.h>

namespace multipass
{
namespace test
{

namespace mp = multipass;

struct MockAvailabilityZone : public mp::AvailabilityZone
{
    MOCK_METHOD(const std::string&, get_name, (), (const, override));
    MOCK_METHOD(const Subnet&, get_subnet, (), (const, override));
    MOCK_METHOD(bool, is_available, (), (const, override));
    MOCK_METHOD(void, set_available, (bool), (override));
    MOCK_METHOD(void, add_vm, (mp::VirtualMachine&), (override));
    MOCK_METHOD(void, remove_vm, (mp::VirtualMachine&), (override));
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_AVAILABILITY_ZONE_H
