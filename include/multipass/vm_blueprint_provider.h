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

#ifndef MULTIPASS_VM_BLUEPRINT_PROVIDER_H
#define MULTIPASS_VM_BLUEPRINT_PROVIDER_H

#include "client_launch_data.h"
#include "disabled_copy_move.h"
#include "query.h"
#include "virtual_machine_description.h"
#include "vm_image_info.h"

#include <optional>
#include <string>
#include <vector>

namespace multipass
{
class VMBlueprintProvider : private DisabledCopyMove
{
public:
    virtual ~VMBlueprintProvider() = default;

    virtual Query fetch_blueprint_for(const std::string& blueprint_name, VirtualMachineDescription& vm_desc,
                                      ClientLaunchData& client_launch_data) = 0;
    virtual Query blueprint_from_file(const std::string& path, const std::string& blueprint_name,
                                      VirtualMachineDescription& vm_desc, ClientLaunchData& client_launch_data) = 0;
    virtual std::optional<VMImageInfo> info_for(const std::string& blueprint_name) = 0;
    virtual std::vector<VMImageInfo> all_blueprints() = 0;
    virtual std::string name_from_blueprint(const std::string& blueprint_name) = 0;
    virtual int blueprint_timeout(const std::string& blueprint_name) = 0;

protected:
    VMBlueprintProvider() = default;
};
} // namespace multipass
#endif // MULTIPASS_VM_BLUEPRINT_PROVIDER_H
