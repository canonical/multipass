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

#include "multipass/base_availability_zone.h"
#include "multipass/exceptions/availability_zone_exceptions.h"
#include "multipass/file_ops.h"
#include "multipass/logging/log.h"

#include <fmt/format.h>

#include <QJsonDocument>

namespace multipass
{

namespace mpl = logging;

constexpr auto subnet_key = "subnet";
constexpr auto available_key = "available";

BaseAvailabilityZone::BaseAvailabilityZone(const std::string& name, const fs::path& az_directory)
    : name{name}, file_path{az_directory / (name + ".json")}
{
    mpl::log(mpl::Level::info, name, "creating zone");

    std::error_code err;
    const auto file_type = MP_FILEOPS.status(file_path, err).type();
    if (err && file_type != fs::file_type::not_found)
    {
        throw AvailabilityZoneDeserializationError{
            "AZ file {:?} is not accessible: {}.",
            file_path.string(),
            err.message(),
        };
    }

    if (file_type == fs::file_type::not_found)
    {
        mpl::log(mpl::Level::info, name, fmt::format("AZ file {:?} not found, using defaults", file_path.string()));
        // TODO GET ACTUAL SUBNET
        subnet = "";
        available = true;
        serialize();
        return;
    }

    if (file_type != fs::file_type::regular)
        throw AvailabilityZoneDeserializationError{"AZ file {:?} is not a regular file.", file_path.string()};

    mpl::log(mpl::Level::info, name, fmt::format("reading AZ from file {:?}", file_path.string()));
    const auto file = MP_FILEOPS.open_read(file_path);
    if (file->fail())
    {
        throw AvailabilityZoneDeserializationError{
            "failed to open AZ file {:?} for reading: {}",
            file_path.string(),
            std::strerror(errno),
        };
    }

    const auto data = QString::fromStdString(std::string{std::istreambuf_iterator{*file}, {}}).toUtf8();
    const auto json = QJsonDocument::fromJson(data).object();

    if (const auto json_subnet = json[subnet_key].toString().toStdString(); json_subnet.empty())
    {
        mpl::log(mpl::Level::warning,
                 name,
                 fmt::format("subnet missing from AZ file {:?}, using default", file_path.string()));
        // TODO GET ACTUAL SUBNET
        subnet = "";
    }
    else
    {
        subnet = json_subnet;
    }

    if (const auto json_available = json[available_key]; !json_available.isBool())
    {
        mpl::log(mpl::Level::warning,
                 name,
                 fmt::format("availability missing from AZ file {:?}, using default", file_path.string()));
        available = true;
    }
    else
    {
        available = json_available.toBool();
    }

    serialize();
}

const std::string& BaseAvailabilityZone::get_name() const
{
    return name;
}

const std::string& BaseAvailabilityZone::get_subnet() const
{
    return subnet;
}

bool BaseAvailabilityZone::is_available() const
{
    const std::unique_lock lock{mutex};
    return available;
}

void BaseAvailabilityZone::set_available(const bool new_available)
{

    const std::unique_lock lock{mutex};
    if (available == new_available)
        return;

    mpl::log(mpl::Level::info, name, fmt::format("making AZ {}available", new_available ? "" : "un"));
    available = new_available;
    serialize();

    for (auto& vm : vms)
        vm.get().make_available(available);
}

void BaseAvailabilityZone::add_vm(VirtualMachine& vm)
{
    const std::unique_lock lock{mutex};
    mpl::log(mpl::Level::info, name, fmt::format("adding vm {:?} to AZ", vm.vm_name));
    vms.emplace_back(vm);
}

void BaseAvailabilityZone::remove_vm(VirtualMachine& vm)
{
    const std::unique_lock lock{mutex};
    mpl::log(mpl::Level::info, name, fmt::format("removing vm {:?} from AZ", vm.vm_name));
    // as of now, we use vm names to uniquely identify vms, so we can do the same here
    const auto to_remove = std::remove_if(vms.begin(), vms.end(), [&](const auto& some_vm) {
        return some_vm.get().vm_name == vm.vm_name;
    });
    vms.erase(to_remove, vms.end());
}

void BaseAvailabilityZone::serialize() const
{
    const std::unique_lock lock{mutex};
    mpl::log(mpl::Level::info, name, fmt::format("writing AZ to file {:?}", file_path.string()));

    const QJsonObject json{
        {subnet_key, QString::fromStdString(subnet)},
        {available_key, available},
    };
    const auto json_bytes = QJsonDocument{json}.toJson();

    const auto file = MP_FILEOPS.open_write(file_path);
    if (file->fail())
    {
        throw AvailabilityZoneSerializationError{
            "failed to open AZ file {:?} for writing: {}",
            file_path.string(),
            std::strerror(errno),
        };
    }

    if (file->write(json_bytes.data(), json_bytes.size()).fail())
    {
        throw AvailabilityZoneSerializationError{
            "failed to write to AZ file {:?}: {}",
            file_path.string(),
            std::strerror(errno),
        };
    }
}
} // namespace multipass
