/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/vm_workflow_provider.h>

namespace multipass
{
namespace test
{
struct StubVMWorkflowProvider final : public VMWorkflowProvider
{
    Query fetch_workflow_for(const std::string& workflow_name, VirtualMachineDescription& vm_desc) override
    {
        throw std::out_of_range("");
    }

    VMImageInfo info_for(const std::string& workflow_name) override
    {
        return VMImageInfo();
    }

    std::vector<VMImageInfo> all_workflows() override
    {
        return {};
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_WORKFLOW_PROVIDER
