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

#ifndef MULTIPASS_BASE_AVAILABILITY_ZONE_H
#define MULTIPASS_BASE_AVAILABILITY_ZONE_H

#include "availability_zone.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace multipass
{
namespace fs = std::filesystem;

class BaseAvailabilityZone : public AvailabilityZone
{
public:
    BaseAvailabilityZone(const std::string& name, const fs::path& az_directory);

    const std::string& get_name() const override;

    const std::string& get_subnet() const override;

    bool is_available() const override;

    void set_available(bool new_available) override;

    void add_vm(VirtualMachine& vm) override;

    void remove_vm(VirtualMachine& vm) override;

private:
    void serialize() const;

    const std::string name;
    const fs::path file_path;
    std::string subnet{};

    mutable std::recursive_mutex mutex{};
    bool available{};
    std::vector<std::reference_wrapper<VirtualMachine>> vms{};
};
} // namespace multipass

#endif // MULTIPASS_BASE_AVAILABILITY_ZONE_H
