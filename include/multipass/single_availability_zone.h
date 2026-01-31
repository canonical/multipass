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

#pragma once

#include "multipass/availability_zone.h"

namespace multipass
{
// When removing the AZ feature flag, move this to tests/unit/stub_availability_zone.h.
class SingleAvailabilityZone final : public AvailabilityZone
{
public:
    SingleAvailabilityZone(std::string name, Subnet subnet)
        : name(std::move(name)), subnet(std::move(subnet))
    {
    }

    const std::string& get_name() const override
    {
        return name;
    }

    const Subnet& get_subnet() const override
    {
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

private:
    std::string name;
    Subnet subnet;
};
} // namespace multipass
