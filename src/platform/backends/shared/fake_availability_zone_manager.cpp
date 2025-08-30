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

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/fake_availability_zone_manager.h>
#include <multipass/virtual_machine.h>

namespace multipass
{

FakeAvailabilityZone::FakeAvailabilityZone(const std::string& name)
    : name{name}, subnet{"10.0.0.0/24"}, available{true}
{
}

const std::string& FakeAvailabilityZone::get_name() const
{
    return name;
}

const std::string& FakeAvailabilityZone::get_subnet() const
{
    return subnet;
}

bool FakeAvailabilityZone::is_available() const
{
    return available;
}

void FakeAvailabilityZone::set_available(bool new_available)
{
    throw NotImplementedOnThisBackendException("availability zones");
}

void FakeAvailabilityZone::add_vm(VirtualMachine& vm)
{
    // No-op for fake zone
}

void FakeAvailabilityZone::remove_vm(VirtualMachine& vm)
{
    // No-op for fake zone
}

FakeAvailabilityZoneManager::FakeAvailabilityZoneManager()
    : zone1{std::make_unique<FakeAvailabilityZone>("zone1")}
{
}

AvailabilityZone& FakeAvailabilityZoneManager::get_zone(const std::string& name)
{
    // For Hyper-V, only zone1 is supported
    if (name != "zone1")
    {
        throw NotImplementedOnThisBackendException("availability zones");
    }
    return *zone1;
}

std::vector<std::reference_wrapper<const AvailabilityZone>> FakeAvailabilityZoneManager::get_zones()
{
    throw NotImplementedOnThisBackendException("availability zones");
}

std::string FakeAvailabilityZoneManager::get_automatic_zone_name()
{
    // For Hyper-V, we always return zone1
    return "zone1";
}

std::string FakeAvailabilityZoneManager::get_default_zone_name() const
{
    // For Hyper-V, we always return zone1
    return "zone1";
}

} // namespace multipass
