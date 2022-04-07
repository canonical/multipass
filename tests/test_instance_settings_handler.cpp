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
enum class SpecialInstanceState
{
    none,
    preparing,
    deleted
};
using InstanceName = const char*;

struct TestInstanceSettingsHandler : public Test
{
    mp::InstanceSettingsHandler make_handler(std::function<void()> instance_persister = [] {})
    {
        return mp::InstanceSettingsHandler{specs, vms, deleted_vms, preparing_vms, instance_persister};
    }

    void fake_instance_state(const char* name, SpecialInstanceState special_state)
    {
        if (special_state == SpecialInstanceState::preparing)
            preparing_vms.emplace(name);
        else if (special_state == SpecialInstanceState::deleted)
            deleted_vms[name];
    }

    std::function<void()> make_fake_persister()
    {
        return [this] { fake_persister_called = true; };
    }

    // empty components to fill before creating handler
    std::unordered_map<std::string, mp::VMSpecs> specs;
    std::unordered_map<std::string, mp::VirtualMachine::ShPtr> vms;
    std::unordered_map<std::string, mp::VirtualMachine::ShPtr> deleted_vms;
    std::unordered_set<std::string> preparing_vms;
    bool fake_persister_called = false;
    inline static constexpr auto properties = std::array{"cpus", "disk", "memory"};
};

QString make_key(const QString& instance_name, const QString& property)
{
    return QString("%1.%2.%3").arg(mp::daemon_settings_root, instance_name, property);
}

using Instance = std::tuple<InstanceName, SpecialInstanceState>;
using Instances = std::vector<Instance>;
using InstanceLists = std::vector<Instances>;

struct TestInstanceSettingsKeys : public TestInstanceSettingsHandler, public WithParamInterface<Instances>
{
};

TEST_P(TestInstanceSettingsKeys, keysCoversAllPropertiesForAllInstances)
{
    const auto intended_instances = GetParam();
    auto expected_keys = std::vector<QString>{};

    for (const auto& [name, special_state] : intended_instances)
    {
        specs[name];
        fake_instance_state(name, special_state);

        for (const auto& prop : properties)
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

    make_handler(make_fake_persister()).keys();
    EXPECT_FALSE(fake_persister_called);
}

TEST_F(TestInstanceSettingsHandler, getFetchesInstanceCPUs)
{
    constexpr auto target_instance_name = "foo";
    specs.insert({{target_instance_name, {}}, {"bar", {}}, {"baz", {}}});
    specs[target_instance_name].num_cores = 78;

    auto got = make_handler().get(make_key(target_instance_name, "cpus"));
    EXPECT_EQ(got, QString::number(specs[target_instance_name].num_cores));
}

TEST_F(TestInstanceSettingsHandler, getFetchesInstanceMemory)
{
    constexpr auto target_instance_name = "elsa";
    specs.insert({{"hugo", {}}, {target_instance_name, {}}, {"flint", {}}});
    specs[target_instance_name].mem_size = mp::MemorySize{"789MiB"};

    auto got = make_handler().get(make_key(target_instance_name, "memory"));
    got.remove(".0"); // TODO drop decimal removal once MemorySize accepts it as input
    EXPECT_EQ(mp::MemorySize{got.toStdString()}, specs[target_instance_name].mem_size); /* note that this doesn't work
    for all values, because the value is returned in human readable format, which approximates (unless and until --raw
    is used/implemented) */
}

TEST_F(TestInstanceSettingsHandler, getFetchesInstanceDisk)
{
    constexpr auto target_instance_name = "blue";
    specs.insert({{"rhapsody", {}}, {"in", {}}, {target_instance_name, {}}});
    specs[target_instance_name].disk_space = mp::MemorySize{"123G"};

    auto got = make_handler().get(make_key(target_instance_name, "disk"));
    got.remove(".0");                                                                     // TODO idem
    EXPECT_EQ(mp::MemorySize{got.toStdString()}, specs[target_instance_name].disk_space); // idem
}

TEST_F(TestInstanceSettingsHandler, getReturnsMemorySizesInHumanReadableFormat)
{
    constexpr auto target_instance_name = "tinkerbell-hates-goatees";
    specs[target_instance_name].disk_space = mp::MemorySize{"12345KiB"};
    specs[target_instance_name].mem_size = mp::MemorySize{"345678"};

    const auto handler = make_handler();

    EXPECT_EQ(handler.get(make_key(target_instance_name, "disk")), "12.1MiB");
    EXPECT_EQ(handler.get(make_key(target_instance_name, "memory")), "337.6KiB");
}

TEST_F(TestInstanceSettingsHandler, getFetchesPropertiesOfInstanceInSpecialState)
{
    constexpr auto preparing_instance = "nouvelle", deleted_instance = "vague";
    specs[preparing_instance];
    specs[deleted_instance];

    fake_instance_state(preparing_instance, SpecialInstanceState::preparing);
    fake_instance_state(deleted_instance, SpecialInstanceState::deleted);

    const auto handler = make_handler();

    for (const auto& instance : {preparing_instance, deleted_instance})
    {
        for (const auto& property : properties)
        {
            EXPECT_NO_THROW(handler.get(make_key(instance, property)));
        }
    }
}

TEST_F(TestInstanceSettingsHandler, getDoesNotPersistInstances)
{
    constexpr auto ready_instance = "asdf", preparing_instance = "sdfg", deleted_instance = "dfgh";
    constexpr auto instances = std::array{ready_instance, preparing_instance, deleted_instance};

    for (const auto& instance : instances)
        specs[instance];

    fake_instance_state(preparing_instance, SpecialInstanceState::preparing);
    fake_instance_state(deleted_instance, SpecialInstanceState::deleted);

    const auto handler = make_handler(make_fake_persister());

    auto property_it = std::begin(properties);
    for (const auto& instance : instances)
    {
        assert(property_it != std::end(properties));
        handler.get(make_key(instance, *property_it++));
    }

    EXPECT_FALSE(fake_persister_called);
}

TEST_F(TestInstanceSettingsHandler, constOperationsDoNotModifyInstances) /* note that `const` on the respective methods
isn't enough for the compiler to catch changes to vms and specs, which live outside of the handler (only references held
there) */
{
    constexpr auto make_mem_size = [](int gigs) { return mp::MemorySize{fmt::format("{}GiB", gigs)}; };
    auto gigs = 1;

    mp::VMSpecs spec;
    spec.num_cores = 3;
    spec.mem_size = make_mem_size(gigs);
    spec.ssh_username = "hugo";
    spec.default_mac_address = "+++++";

    for (const auto& name : {"toto", "tata", "fuzz"})
    {
        vms.insert({name, {}});
        specs.insert({name, spec});
        spec.mem_size = make_mem_size(++gigs);
        ++spec.num_cores;
    }

    auto specs_copy = specs;
    auto vms_copy = vms;

    auto handler = make_handler();
    for (const auto& key : handler.keys())
        handler.get(key);

    EXPECT_EQ(specs, specs_copy);
    EXPECT_EQ(vms, vms_copy);
}

TEST_F(TestInstanceSettingsHandler, getThrowsOnMissingInstance)
{
    constexpr auto instance = "missing-instance";

    const auto handler = make_handler();

    for (const auto& prop : properties)
    {
        MP_EXPECT_THROW_THAT(handler.get(make_key(instance, prop)), mp::InstanceSettingsException,
                             mpt::match_what(AllOf(HasSubstr(instance), HasSubstr("No such instance"))));
    }
}

TEST_F(TestInstanceSettingsHandler, getThrowsOnWrongProperty)
{
    constexpr auto target_instance_name = "asdf";
    constexpr auto wrong_property = "wrong";
    specs[target_instance_name];

    MP_EXPECT_THROW_THAT(make_handler().get(make_key(target_instance_name, wrong_property)),
                         mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(wrong_property)));
}
} // namespace
