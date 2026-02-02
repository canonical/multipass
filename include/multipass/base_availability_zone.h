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
class BaseAvailabilityZone : public AvailabilityZone
{
public:
    BaseAvailabilityZone(const std::string& name,
                         size_t num,
                         const std::filesystem::path& az_directory);

    const std::string& get_name() const override;
    const Subnet& get_subnet() const override;
    bool is_available() const override;
    void set_available(bool new_available) override;
    void add_vm(VirtualMachine& vm) override;
    void remove_vm(VirtualMachine& vm) override;

private:
    void serialize() const;

    mutable std::recursive_mutex mutex;
    const std::filesystem::path file_path;
    const std::string name;
    std::vector<std::reference_wrapper<VirtualMachine>> vms;

    // we store all the data in one struct so that it can be created from one function call in the
    // initializer list
    struct Data
    {
        const Subnet subnet;
        bool available{};
    } m;

    static Data read_from_file(const std::string& name,
                               size_t zone_num,
                               const std::filesystem::path& file_path);
};
} // namespace multipass

#endif // MULTIPASS_BASE_AVAILABILITY_ZONE_H
