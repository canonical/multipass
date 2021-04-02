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

#ifndef MULTIPASS_VM_WORKFLOW_PROVIDER_H
#define MULTIPASS_VM_WORKFLOW_PROVIDER_H

#include <multipass/query.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_image_info.h>

#include <string>
#include <vector>

namespace multipass
{
class VMWorkflowProvider
{
public:
    virtual ~VMWorkflowProvider() = default;

    virtual Query fetch_workflow_for(const std::string& workflow_name, VirtualMachineDescription& vm_desc) = 0;
    virtual VMImageInfo info_for(const std::string& workflow_name) = 0;
    virtual std::vector<VMImageInfo> all_workflows() = 0;

protected:
    VMWorkflowProvider() = default;
    VMWorkflowProvider(const VMWorkflowProvider&) = delete;
    VMWorkflowProvider& operator=(const VMWorkflowProvider&) = delete;
};
} // namespace multipass
#endif // MULTIPASS_VM_WORKFLOW_PROVIDER_H
