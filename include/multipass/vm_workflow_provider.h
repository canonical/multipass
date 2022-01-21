/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#include "disabled_copy_move.h"
#include "query.h"
#include "virtual_machine_description.h"
#include "vm_image_info.h"

#include <string>
#include <vector>

namespace multipass
{
class VMWorkflowProvider : private DisabledCopyMove
{
public:
    virtual ~VMWorkflowProvider() = default;

    virtual Query fetch_workflow_for(const std::string& workflow_name, VirtualMachineDescription& vm_desc) = 0;
    virtual VMImageInfo info_for(const std::string& workflow_name) = 0;
    virtual std::vector<VMImageInfo> all_workflows() = 0;
    virtual std::string name_from_workflow(const std::string& workflow_name) = 0;
    virtual int workflow_timeout(const std::string& workflow_name) = 0;

protected:
    VMWorkflowProvider() = default;
};
} // namespace multipass
#endif // MULTIPASS_VM_WORKFLOW_PROVIDER_H
