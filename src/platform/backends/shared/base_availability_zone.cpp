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
#include "multipass/file_ops.h"
#include "multipass/logging/log.h"

#include <fmt/format.h>

#include <QJsonDocument>

namespace multipass
{

namespace mpl = logging;

BaseAvailabilityZone::BaseAvailabilityZone(const std::string& name, const fs::path& az_directory)
    : name{name}, file_path{az_directory / (name + ".json")}
{
    mpl::log(mpl::Level::info, name, "creating zone");

    std::error_code err;
    const auto file_type = MP_FILEOPS.status(file_path, err).type();
    if (err && file_type != fs::file_type::not_found)
    {
        const auto message = fmt::format("AZ file {:?} is not accessible: {}.", file_path.string(), err.message());
        throw std::runtime_error(message);
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
        throw std::runtime_error(fmt::format("AZ file {:?} is not a regular file.", file_path.string()));

    mpl::log(mpl::Level::info, name, fmt::format("reading AZ from file {:?}", file_path.string()));
    const auto file = MP_FILEOPS.open_read(file_path);
    if (file->fail())
    {
        const auto message =
            fmt::format("failed to open AZ file {:?} for reading: {}", file_path.string(), std::strerror(errno));
        throw std::runtime_error{message};
    }
    const std::string data{std::istreambuf_iterator{*file}, {}};
    const auto qdata = QString::fromStdString(data).toUtf8();
    const auto json = QJsonDocument::fromJson(qdata).object();

    const auto json_subnet = json["subnet"].toString();
    if (json_subnet.isEmpty())
    {
        mpl::log(mpl::Level::warning,
                 name,
                 fmt::format("subnet missing from AZ file {:?}, using default", file_path.string()));
        // TODO GET ACTUAL SUBNET
        subnet = "";
    }
    else
    {
        subnet = json_subnet.toStdString();
    }

    const auto json_available = json["available"];
    const auto json_has_available = json_available.isBool();
    if (!json_has_available)
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

void BaseAvailabilityZone::set_available(bool new_available)
{
    constexpr auto available_str = "available";
    constexpr auto unavailable_str = "unavailable";

    const std::unique_lock lock{mutex};
    mpl::log(mpl::Level::info, name, fmt::format("making AZ {}", new_available ? available_str : unavailable_str));
    available = new_available;
    for (auto& vm : vms)
    {
        vm.get().make_available(available);
    }
}

void BaseAvailabilityZone::add_vm(VirtualMachine& vm)
{
    const std::unique_lock lock{mutex};
    mpl::log(mpl::Level::info, name, fmt::format("adding vm {:?} to AZ", vm.vm_name));
    vms.push_back(vm);
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
    QJsonObject json{};
    json["subnet"] = QString::fromStdString(subnet);
    json["available"] = available;
    const auto json_bytes = QJsonDocument{json}.toJson();
    const auto file = MP_FILEOPS.open_write(file_path);
    if (file->fail())
    {
        const auto message =
            fmt::format("failed to open AZ file {:?} for writing: {}", file_path.string(), std::strerror(errno));
        throw std::runtime_error{message};
    }
    if (file->write(json_bytes.data(), json_bytes.size()).fail())
    {
        const auto message = fmt::format("failed write to AZ file {:?}: {}", file_path.string(), std::strerror(errno));
        throw std::runtime_error{message};
    }
}
} // namespace multipass
