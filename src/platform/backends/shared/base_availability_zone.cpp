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

#include <multipass/base_availability_zone.h>
#include <multipass/exceptions/availability_zone_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <scope_guard.hpp>

#include <QJsonDocument>

namespace mpl = multipass::logging;

namespace
{
constexpr auto subnet_key = "subnet";
constexpr auto available_key = "available";

[[nodiscard]] QJsonObject read_json(const multipass::fs::path& file_path, const std::string& name)
try
{
    return MP_JSONUTILS.read_object_from_file(file_path);
}
catch (const std::ios_base::failure& e)
{
    mpl::warn(name, "failed to read AZ file: {}", e.what());
    return QJsonObject{};
}

[[nodiscard]] std::string deserialize_subnet(const QJsonObject& json,
                                             const multipass::fs::path& file_path,
                                             const std::string& name)
{
    if (const auto json_subnet = json[subnet_key].toString().toStdString(); !json_subnet.empty())
        return json_subnet;

    mpl::debug(name, "subnet missing from AZ file '{}', using default", file_path);
    // TODO GET ACTUAL SUBNET
    return {};
};

[[nodiscard]] bool deserialize_available(const QJsonObject& json,
                                         const multipass::fs::path& file_path,
                                         const std::string& name)
{
    if (const auto json_available = json[available_key]; json_available.isBool())
        return json_available.toBool();

    mpl::debug(name, "availability missing from AZ file '{}', using default", file_path);
    return true;
}
} // namespace

namespace multipass
{

BaseAvailabilityZone::data BaseAvailabilityZone::read_from_file(const std::string& name,
                                                                const fs::path& file_path)
{
    mpl::trace(name, "reading AZ from file '{}'", file_path);

    const auto json = read_json(file_path, name);
    return {
        .name = name,
        .file_path = file_path,
        .subnet = deserialize_subnet(json, file_path, name),
        .available = deserialize_available(json, file_path, name),
    };
}

BaseAvailabilityZone::BaseAvailabilityZone(const std::string& name, const fs::path& az_directory)
    : m{read_from_file(name, az_directory / (name + ".json"))}
{
    serialize();
}

const std::string& BaseAvailabilityZone::get_name() const
{
    return m.name;
}

const std::string& BaseAvailabilityZone::get_subnet() const
{
    return m.subnet;
}

bool BaseAvailabilityZone::is_available() const
{
    const std::unique_lock lock{m.mutex};
    return m.available;
}

void BaseAvailabilityZone::set_available(const bool new_available)
{

    mpl::debug(m.name, "making AZ {}available", new_available ? "" : "un");
    const std::unique_lock lock{m.mutex};
    if (m.available == new_available)
        return;

    m.available = new_available;
    auto serialize_guard = sg::make_scope_guard([this]() noexcept {
        try
        {
            serialize();
        }
        catch (const std::exception& e)
        {
            mpl::error(m.name, "Failed to serialize availability zone: {}", e.what());
        }
    });

    try
    {
        for (auto& vm : m.vms)
            vm.get().set_available(new_available);
    }
    catch (...)
    {
        // if an error occurs fallback to available.
        m.available = true;

        // make sure nothing is still unavailable.
        for (auto& vm : m.vms)
        {
            // setting the state here breaks encapsulation, but it's already broken.
            std::unique_lock vm_lock{vm.get().state_mutex};
            if (vm.get().current_state() == VirtualMachine::State::unavailable)
            {
                vm.get().state = VirtualMachine::State::off;
                vm.get().update_state();
            }
        }

        // rethrow the error so something else can deal with it.
        throw;
    }
}

void BaseAvailabilityZone::add_vm(VirtualMachine& vm)
{
    mpl::debug(m.name, "adding vm '{}' to AZ", vm.vm_name);
    const std::unique_lock lock{m.mutex};
    m.vms.emplace_back(vm);
}

void BaseAvailabilityZone::remove_vm(VirtualMachine& vm)
{
    mpl::debug(m.name, "removing vm '{}' from AZ", vm.vm_name);
    const std::unique_lock lock{m.mutex};
    // as of now, we use vm names to uniquely identify vms, so we can do the same here
    const auto to_remove = std::remove_if(m.vms.begin(), m.vms.end(), [&](const auto& some_vm) {
        return some_vm.get().vm_name == vm.vm_name;
    });
    m.vms.erase(to_remove, m.vms.end());
}

void BaseAvailabilityZone::serialize() const
{
    mpl::trace(m.name, "writing AZ to file '{}'", m.file_path);
    const std::unique_lock lock{m.mutex};

    const QJsonObject json{
        {subnet_key, QString::fromStdString(m.subnet)},
        {available_key, m.available},
    };

    MP_JSONUTILS.write_json(json, QString::fromStdU16String(m.file_path.u16string()));
}
} // namespace multipass
