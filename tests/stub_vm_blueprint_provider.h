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
#ifndef MULTIPASS_STUB_WORKFLOW_PROVIDER
#define MULTIPASS_STUB_WORKFLOW_PROVIDER

#include <multipass/exceptions/blueprint_exceptions.h>
#include <multipass/vm_blueprint_provider.h>

namespace multipass
{
namespace test
{
struct StubVMBlueprintProvider final : public VMBlueprintProvider
{
    Query fetch_blueprint_for(const std::string& blueprint_name, VirtualMachineDescription& vm_desc,
                              ClientLaunchData& client_launch_data) override
    {
        throw std::out_of_range("");
    }

    Query blueprint_from_file(const std::string& path, const std::string& blueprint_name,
                              VirtualMachineDescription& vm_desc, ClientLaunchData& client_launch_data) override
    {
        throw InvalidBlueprintException("");
    }

    std::optional<VMImageInfo> info_for(const std::string& blueprint_name) override
    {
        return VMImageInfo();
    }

    std::vector<VMImageInfo> all_blueprints() override
    {
        return {};
    }

    std::string name_from_blueprint(const std::string& blueprint_name) override
    {
        return {};
    }

    int blueprint_timeout(const std::string& blueprint_name) override
    {
        return 0;
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_WORKFLOW_PROVIDER
