/*
 * Copyright (C) 2022 Canonical, Ltd.
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

#include "common.h"

#include <multipass/virtual_machine.h>

#include <src/daemon/instance_settings_handler.h>
#include <src/daemon/vm_specs.h>

#include <QString>

#include <string>
#include <unordered_map>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
struct TestInstanceSettingsKeys : public TestWithParam<std::vector<const char*>>
{
};

TEST_P(TestInstanceSettingsKeys, keysCoversAllPropertiesForAllInstances)
{
    constexpr auto props = std::array{"cpus", "disk", "memory"};
    const auto names = GetParam();

    auto vms = std::unordered_map<std::string, mp::VirtualMachine::ShPtr>{};
    auto specs = std::unordered_map<std::string, mp::VMSpecs>{};
    auto expected_keys = std::vector<QString>{};

    for (const auto& name : names)
    {
        specs[name] = {};
        for (const auto& prop : props)
            expected_keys.push_back(QString{"local.%1.%2"}.arg(name, prop));
    }

    auto handler = mp::InstanceSettingsHandler{specs, vms, /*deleted_instances=*/{}, /*preparing_instances=*/{},
                                               /*instance_persister=*/[] {}};

    EXPECT_THAT(handler.keys(), UnorderedElementsAreArray(expected_keys));
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsHandler, TestInstanceSettingsKeys,
                         ValuesIn(std::vector<std::vector<const char*>>{{},
                                                                        {"morning-light-mountain"},
                                                                        {"foo", "bar", "MORNING-LIGHT-MOUNTAIN"},
                                                                        {"a", "b", "c", "d", "e", "f", "g"}}));
} // namespace
