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

#include <multipass/stub_availability_zone.h>

namespace multipass
{

StubAvailabilityZone::StubAvailabilityZone(const PrivatePass& pass) noexcept
    : AvailabilityZone{}, Singleton<StubAvailabilityZone>{pass}, name{}, subnet{"0.0.0.0/0"}
{
}

const std::string& StubAvailabilityZone::get_name() const
{
    return name;
}

const Subnet& StubAvailabilityZone::get_subnet() const
{
    return subnet;
}

bool StubAvailabilityZone::is_available() const
{
    return true;
}

void StubAvailabilityZone::set_available(bool)
{
    // no-op: backends that use this stub do not support availability zones, so this zone can
    // never be disabled.
}

void StubAvailabilityZone::add_vm(VirtualMachine&)
{
    // no-op: no zone-availability tracking is needed for backends that use this stub.
}

void StubAvailabilityZone::remove_vm(VirtualMachine&)
{
    // no-op: no zone-availability tracking is needed for backends that use this stub.
}

} // namespace multipass
