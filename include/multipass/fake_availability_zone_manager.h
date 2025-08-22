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

#ifndef MULTIPASS_FAKE_AVAILABILITY_ZONE_MANAGER_H
#define MULTIPASS_FAKE_AVAILABILITY_ZONE_MANAGER_H

#include "availability_zone.h"
#include "availability_zone_manager.h"
#include "exceptions/not_implemented_on_this_backend_exception.h"

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
class FakeAvailabilityZone : public AvailabilityZone
{
public:
    FakeAvailabilityZone(const std::string& name);

    [[nodiscard]] const std::string& get_name() const override;
    [[nodiscard]] const std::string& get_subnet() const override;
    [[nodiscard]] bool is_available() const override;
    void set_available(bool new_available) override;
    void add_vm(VirtualMachine& vm) override;
    void remove_vm(VirtualMachine& vm) override;

private:
    std::string name;
    std::string subnet;
    bool available;
};

class FakeAvailabilityZoneManager : public AvailabilityZoneManager
{
public:
    FakeAvailabilityZoneManager();

    AvailabilityZone& get_zone(const std::string& name) override;
    std::vector<std::reference_wrapper<const AvailabilityZone>> get_zones() override;
    std::string get_automatic_zone_name() override;
    std::string get_default_zone_name() const override;

private:
    std::unique_ptr<FakeAvailabilityZone> zone1;
};

} // namespace multipass

#endif // MULTIPASS_FAKE_AVAILABILITY_ZONE_MANAGER_H
