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

StubAvailabilityZone::StubAvailabilityZone() noexcept
    : AvailabilityZone{}, name{"zone1"}, subnet{"0.0.0.0/0"}
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

void StubAvailabilityZone::set_available(bool /*new_available*/)
{
}

} // namespace multipass
