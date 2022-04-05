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

#include <multipass/constants.h>
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
struct TestInstanceSettingsHandler : public Test
{
    mp::InstanceSettingsHandler make_handler(std::function<void()> instance_persister = [] {})
    {
        return mp::InstanceSettingsHandler{specs, vms, deleted_vms, preparing_vms, instance_persister};
    }

    // empty components to fill before creating handler
    std::unordered_map<std::string, mp::VMSpecs> specs;
    std::unordered_map<std::string, mp::VirtualMachine::ShPtr> vms;
    std::unordered_map<std::string, mp::VirtualMachine::ShPtr> deleted_vms;
    std::unordered_set<std::string> preparing_vms;
};

QString make_key(const QString& instance_name, const QString& property)
{
    return QString("%1.%2.%3").arg(mp::daemon_settings_root, instance_name, property);
}

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

struct TestInstanceSettingsKeys : public TestInstanceSettingsHandler, public WithParamInterface<Instances>
{
};

TEST_P(TestInstanceSettingsKeys, keysCoversAllPropertiesForAllInstances)
{
    constexpr auto props = std::array{"cpus", "disk", "memory"};
    const auto intended_instances = GetParam();
    auto expected_keys = std::vector<QString>{};

    for (const auto& [name, special_state] : intended_instances)
    {
        specs[name];

        if (special_state == SpecialInstanceState::preparing)
            preparing_vms.emplace(name);
        else if (special_state == SpecialInstanceState::deleted)
            deleted_vms[name];

        for (const auto& prop : props)
            expected_keys.push_back(make_key(name, prop));
    }

    EXPECT_THAT(make_handler().keys(), UnorderedElementsAreArray(expected_keys));
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

TEST_F(TestInstanceSettingsHandler, keysDoesNotPersistInstances)
{
    specs.insert({{"abc", {}}, {"xyz", {}}, {"blah", {}}});
    deleted_vms["blah"];
    preparing_vms.emplace("xyz");

    bool persisted = false;
    make_handler([&persisted] { persisted = true; }).keys();

    EXPECT_FALSE(persisted);
}

TEST_F(TestInstanceSettingsHandler, keysDoesNotModifyInstances)
{
    mp::VMSpecs spec;
    spec.num_cores = 3;
    spec.ssh_username = "hugo";

    for (const auto& name : {"toto", "tata", "fuzz"})
    {
        vms.insert({name, {}});
        specs.insert({name, spec});
        ++spec.num_cores;
    }

    auto specs_copy = specs;
    auto vms_copy = vms;

    make_handler().keys();

    EXPECT_EQ(specs, specs_copy);
    EXPECT_EQ(vms, vms_copy);
}

TEST_F(TestInstanceSettingsHandler, getFetchesInstanceCPUs)
{
    constexpr auto target_instance_name = "foo";

    for (const auto& name : {target_instance_name, "bar", "baz"})
    {
        specs.insert({name, {}});
        vms.insert({name, {}});
    }

    specs[target_instance_name].num_cores = 78;

    auto got = make_handler().get(make_key(target_instance_name, "cpus"));
    EXPECT_EQ(got, QString::number(specs[target_instance_name].num_cores));
}
} // namespace
