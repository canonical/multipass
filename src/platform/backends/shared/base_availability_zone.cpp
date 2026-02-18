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

const multipass::Subnet subnet_range{"10.97.0.0/20"};
constexpr auto subnet_prefix_length = 24;
} // namespace

namespace multipass
{

BaseAvailabilityZone::BaseAvailabilityZone(const std::string& name,
                                           size_t num,
                                           const fs::path& az_directory)
    : file_path{az_directory / (name + ".json")}, name{name}, m{load_file(name, num, file_path)}
{
    save_file();
}

const std::string& BaseAvailabilityZone::get_name() const
{
    return name;
}

const Subnet& BaseAvailabilityZone::get_subnet() const
{
    return m.subnet;
}

bool BaseAvailabilityZone::is_available() const
{
    const std::unique_lock lock{mutex};
    return m.available;
}

void BaseAvailabilityZone::set_available(const bool new_available)
{

    mpl::debug(name, "making AZ {}available", new_available ? "" : "un");
    const std::unique_lock lock{mutex};
    if (m.available == new_available)
        return;

    m.available = new_available;
    auto save_file_guard = sg::make_scope_guard([this]() noexcept {
        try
        {
            save_file();
        }
        catch (const std::exception& e)
        {
            mpl::error(name, "Failed to serialize availability zone: {}", e.what());
        }
    });

    try
    {
        for (auto& vm : vms)
            vm.get().set_available(new_available);
    }
    catch (...)
    {
        // if an error occurs fallback to available.
        m.available = true;

        // make sure nothing is still unavailable.
        for (auto& vm : vms)
        {
            // setting the state here breaks encapsulation, but it's already broken.
            std::unique_lock vm_lock{vm.get().state_mutex};
            if (vm.get().current_state() == VirtualMachine::State::unavailable)
            {
                vm.get().state = VirtualMachine::State::off;
                vm.get().handle_state_update();
            }
        }

        // rethrow the error so something else can deal with it.
        throw;
    }
}

void BaseAvailabilityZone::add_vm(VirtualMachine& vm)
{
    mpl::debug(name, "adding vm '{}' to AZ", vm.get_name());
    const std::unique_lock lock{mutex};
    vms.emplace_back(vm);
}

void BaseAvailabilityZone::remove_vm(VirtualMachine& vm)
{
    mpl::debug(name, "removing vm '{}' from AZ", vm.get_name());
    const std::unique_lock lock{mutex};
    // as of now, we use vm names to uniquely identify vms, so we can do the same here
    const auto to_remove = std::remove_if(vms.begin(), vms.end(), [&](const auto& some_vm) {
        return some_vm.get().get_name() == vm.get_name();
    });
    vms.erase(to_remove, vms.end());
}

BaseAvailabilityZone::Data BaseAvailabilityZone::load_file(const std::string& name,
                                                           size_t zone_num,
                                                           const fs::path& file_path)
{
    mpl::trace(name, "reading AZ from file '{}'", file_path);
    if (auto filedata = MP_FILEOPS.try_read_file(file_path))
    {
        try
        {
            auto json = boost::json::parse(*filedata);
            return value_to<Data>(json);
        }
        catch (const boost::system::system_error& e)
        {
            mpl::error("aliases", "Error parsing file '{}': {}", file_path, e.what());
        }
    }
    // Return a default value if we couldn't load from `file_path`.
    return {
        .subnet = subnet_range.get_specific_subnet(zone_num, subnet_prefix_length),
        .available = true,
    };
}

void BaseAvailabilityZone::save_file() const
{
    mpl::trace(name, "writing AZ to file '{}'", file_path);
    const std::unique_lock lock{mutex};

    auto json = boost::json::value_from(m);
    MP_FILEOPS.write_transactionally(QString::fromStdString(file_path.string()),
                                     pretty_print(json));
}

void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const BaseAvailabilityZone::Data& zone)
{
    json = {{subnet_key, boost::json::value_from(zone.subnet)}, {available_key, zone.available}};
}

BaseAvailabilityZone::Data tag_invoke(const boost::json::value_to_tag<BaseAvailabilityZone::Data>&,
                                      const boost::json::value& json)
{
    return {
        .subnet = value_to<Subnet>(json.at(subnet_key)),
        .available = value_to<bool>(json.at(available_key)),
    };
}

} // namespace multipass
