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
#include <tuple>
#include <unordered_map>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
enum class SpecialInstanceState
{
    none,
    preparing,
    deleted
};
using InstanceName = const char*;
using Instance = std::tuple<InstanceName, SpecialInstanceState>;
using Instances = std::vector<Instance>;
using InstanceLists = std::vector<Instances>;

struct TestInstanceSettingsKeys : public TestWithParam<Instances>
{
};

TEST_P(TestInstanceSettingsKeys, keysCoversAllPropertiesForAllInstances)
{
    constexpr auto props = std::array{"cpus", "disk", "memory"};
    const auto intended_instances = GetParam();

    auto specs = std::unordered_map<std::string, mp::VMSpecs>{};
    auto vms = std::unordered_map<std::string, mp::VirtualMachine::ShPtr>{};
    auto deleted_vms = std::unordered_map<std::string, mp::VirtualMachine::ShPtr>{};
    auto preparing_vms = std::unordered_set<std::string>{};
    auto expected_keys = std::vector<QString>{};

    for (const auto& [name, special_state] : intended_instances)
    {
        specs[name];

        if (special_state == SpecialInstanceState::preparing)
            preparing_vms.emplace(name);
        else if (special_state == SpecialInstanceState::deleted)
            deleted_vms[name];

        for (const auto& prop : props)
            expected_keys.push_back(QString{"local.%1.%2"}.arg(name, prop));
    }

    auto handler = mp::InstanceSettingsHandler{specs, vms, deleted_vms, preparing_vms, /*instance_persister=*/[] {}};

    EXPECT_THAT(handler.keys(), UnorderedElementsAreArray(expected_keys));
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsKeysEmpty, TestInstanceSettingsKeys, Values(Instances{}));
INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsKeysSingle, TestInstanceSettingsKeys,
                         Values(Instances{{"morning-light-mountain", SpecialInstanceState::none}},
                                Instances{{"morning-light-mountain", SpecialInstanceState::deleted}},
                                Instances{{"morning-light-mountain", SpecialInstanceState::preparing}}));
INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsKeysMultiple, TestInstanceSettingsKeys,
                         ValuesIn(InstanceLists{{{"foo", SpecialInstanceState::none},
                                                 {"bar", SpecialInstanceState::none},
                                                 {"baz", SpecialInstanceState::none}},
                                                {{"foo", SpecialInstanceState::deleted},
                                                 {"bar", SpecialInstanceState::preparing},
                                                 {"baz", SpecialInstanceState::preparing}},
                                                {{"foo", SpecialInstanceState::deleted},
                                                 {"bar", SpecialInstanceState::none},
                                                 {"baz", SpecialInstanceState::deleted}},
                                                {{"foo", SpecialInstanceState::preparing},
                                                 {"bar", SpecialInstanceState::preparing},
                                                 {"baz", SpecialInstanceState::none}},
                                                {{"foo", SpecialInstanceState::none},
                                                 {"bar", SpecialInstanceState::none},
                                                 {"baz", SpecialInstanceState::preparing}}}));
} // namespace
