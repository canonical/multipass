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

#ifndef MULTIPASS_AVAILABILITY_ZONE_H
#define MULTIPASS_AVAILABILITY_ZONE_H

#include "disabled_copy_move.h"
#include "subnet.h"
#include "virtual_machine.h"

#include <memory>

namespace multipass
{
class AvailabilityZone : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<AvailabilityZone>;
    using ShPtr = std::shared_ptr<AvailabilityZone>;

    virtual ~AvailabilityZone() = default;

    [[nodiscard]] virtual const std::string& get_name() const = 0;
    [[nodiscard]] virtual const Subnet& get_subnet() const = 0;
    [[nodiscard]] virtual bool is_available() const = 0;
    virtual void set_available(bool new_available) = 0;
    virtual void add_vm(VirtualMachine& vm) = 0;
    virtual void remove_vm(VirtualMachine& vm) = 0;
};
} // namespace multipass

#endif // MULTIPASS_AVAILABILITY_ZONE_H
