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

#ifndef MULTIPASS_MOCK_VM_BLUEPRINT_PROVIDER_H
#define MULTIPASS_MOCK_VM_BLUEPRINT_PROVIDER_H

#include "common.h"

#include <multipass/vm_blueprint_provider.h>
#include <multipass/vm_image_info.h>

namespace multipass
{
namespace test
{
class MockVMBlueprintProvider : public VMBlueprintProvider
{
public:
    MockVMBlueprintProvider()
    {
        ON_CALL(*this, info_for(_)).WillByDefault([](const auto& blueprint_name) {
            mp::VMImageInfo info;

            info.aliases.append(QString::fromStdString(blueprint_name));
            info.release_title = QString::fromStdString(fmt::format("This is the {} blueprint", blueprint_name));

            return info;
        });
    };

    MOCK_METHOD3(fetch_blueprint_for, Query(const std::string&, VirtualMachineDescription&, ClientLaunchData&));
    MOCK_METHOD4(blueprint_from_file,
                 Query(const std::string&, const std::string&, VirtualMachineDescription&, ClientLaunchData&));
    MOCK_METHOD1(info_for, std::optional<VMImageInfo>(const std::string&));
    MOCK_METHOD0(all_blueprints, std::vector<VMImageInfo>());
    MOCK_METHOD1(name_from_blueprint, std::string(const std::string&));
    MOCK_METHOD1(blueprint_timeout, int(const std::string&));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_VM_BLUEPRINT_PROVIDER_H
