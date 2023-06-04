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

    MOCK_METHOD(Query, fetch_blueprint_for, (const std::string&, VirtualMachineDescription&, ClientLaunchData&),
                (override));
    MOCK_METHOD(Query, blueprint_from_file,
                (const std::string&, const std::string&, VirtualMachineDescription&, ClientLaunchData&), (override));
    MOCK_METHOD(std::optional<VMImageInfo>, info_for, (const std::string&), (override));
    MOCK_METHOD(std::vector<VMImageInfo>, all_blueprints, (), (override));
    MOCK_METHOD(std::string, name_from_blueprint, (const std::string&), (override));
    MOCK_METHOD(int, blueprint_timeout, (const std::string&), (override));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_VM_BLUEPRINT_PROVIDER_H
