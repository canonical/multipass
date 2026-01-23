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

#ifndef MULTIPASS_STUB_AVAILABILITY_ZONE_H
#define MULTIPASS_STUB_AVAILABILITY_ZONE_H

#include "multipass/availability_zone.h"

namespace multipass
{
namespace test
{
struct StubAvailabilityZone final : public AvailabilityZone
{
    StubAvailabilityZone()
    {
    }

    const std::string& get_name() const override
    {
        static std::string name{"zone1"};
        return name;
    }

    const Subnet& get_subnet() const override
    {
        static Subnet subnet{"192.168.123.0/24"};
        return subnet;
    }

    bool is_available() const override
    {
        return true;
    }

    void set_available(bool new_available) override
    {
    }

    void add_vm(VirtualMachine& vm) override
    {
    }

    void remove_vm(VirtualMachine& vm) override
    {
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_AVAILABILITY_ZONE_H
