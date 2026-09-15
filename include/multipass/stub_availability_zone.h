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

#include <multipass/availability_zone.h>
#include <multipass/singleton.h>

namespace multipass
{
// A no-op, always-available AvailabilityZone used by backends that do not support the concept of
// availability zones (e.g. legacy VirtualBox and Hyper-V). It is never disabled and carries no
// meaningful name or subnet, so it is safe to hand out regardless of any persisted zone state.
class StubAvailabilityZone final : public AvailabilityZone, public Singleton<StubAvailabilityZone>
{
public:
    StubAvailabilityZone(const PrivatePass& pass) noexcept;

    const std::string& get_name() const override;
    const Subnet& get_subnet() const override;
    bool is_available() const override;
    void set_available(bool new_available) override;
    void add_vm(VirtualMachine& vm) override;
    void remove_vm(VirtualMachine& vm) override;

private:
    std::string name;
    Subnet subnet;
};
} // namespace multipass

#endif // MULTIPASS_STUB_AVAILABILITY_ZONE_H
