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

#include "common.h"
#include "mock_virtual_machine.h"

#include <multipass/constants.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_specs.h>

#include <src/daemon/instance_settings_handler.h>

#include <QString>

#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>

namespace mp = multipass;
namespace mpt = mp::test;
namespace mpu = mp::utils;
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
    mp::InstanceSettingsHandler make_handler()
    {
        return mp::InstanceSettingsHandler{specs,
                                           vms,
                                           deleted_vms,
                                           preparing_vms,
                                           make_fake_persister(),
                                           make_fake_is_bridged(),
                                           make_fake_add()};
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

    std::function<std::string()> make_fake_bridged_interface()
    {
        return [this] { return bridged_interface; };
    }

    std::function<std::string()> make_fake_bridge_name()
    {
        return [] { return "br-eth8"; };
    }

    std::function<std::vector<mp::NetworkInterfaceInfo>()> make_fake_host_networks()
    {
        std::vector<mp::NetworkInterfaceInfo> ret;
        ret.push_back(mp::NetworkInterfaceInfo{"eth8", "ethernet", "Ethernet device", {}, true});
        ret.push_back(mp::NetworkInterfaceInfo{"virbr0", "bridge", "Network bridge", {}, false});

        return [ret] { return ret; };
    }

    std::function<bool()> make_fake_authorization()
    {
        return [this]() { return user_authorized; };
    }

    std::function<bool(const std::string&)> make_fake_is_bridged()
    {
        return [this](const std::string& n) {
            const std::string br_interface{"eth8"};
            const std::string br_name{"br-" + br_interface};
            const auto& spec = specs[n];

            return std::any_of(spec.extra_interfaces.cbegin(),
                               spec.extra_interfaces.cend(),
                               [&br_interface, &br_name](const auto& network) -> bool {
                                   return network.id == br_interface || network.id == br_name;
                               });
        };
    }

    std::function<void(const std::string&)> make_fake_add()
    {
        return [this](const std::string& n) {
            if (!make_fake_is_bridged()(n))
                specs[n].extra_interfaces.push_back(mp::NetworkInterface{"eth8", mpu::generate_mac_address(), true});
        };
    }

    template <template <typename /*MockClass*/> typename MockCharacter = ::testing::NiceMock>
    mpt::MockVirtualMachine& mock_vm(const std::string& name, bool deleted = false)
    {
        auto ret = std::make_shared<MockCharacter<mpt::MockVirtualMachine>>(name);

        auto& vm_collection = deleted ? deleted_vms : vms;
        vm_collection.emplace(name, ret);

        return *ret;
    }

    // empty components to fill before creating handler
    std::unordered_map<std::string, mp::VMSpecs> specs;
    std::unordered_map<std::string, mp::VirtualMachine::ShPtr> vms;
    std::unordered_map<std::string, mp::VirtualMachine::ShPtr> deleted_vms;
    std::unordered_set<std::string> preparing_vms;
    std::string bridged_interface{"eth8"};
    bool fake_persister_called = false;
    bool user_authorized = true;
    inline static constexpr std::array numeric_properties{"cpus", "disk", "memory"};
    inline static constexpr std::array boolean_properties{"bridged"};
    inline static constexpr std::array properties{"cpus", "disk", "memory", "bridged"};
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

    make_handler().keys();
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

struct TestBridgedInstanceSettings : public TestInstanceSettingsHandler,
                                     public WithParamInterface<std::pair<std::string, bool>>
{
};

// Since the instance handler calls functions from the daemon, which were replaced here by other ones, this test just
// checks that the correct functions are called from the handler.
TEST_P(TestBridgedInstanceSettings, getFetchesBridged)
{
    const auto [br_interface, bridged] = GetParam();

    constexpr auto target_instance_name = "lemmy";
    specs.insert({{"mikkey", {}}, {"phil", {}}, {target_instance_name, {}}});

    specs[target_instance_name].extra_interfaces = {{br_interface, "52:54:00:12:34:56", true}};

    const auto got = make_handler().get(make_key(target_instance_name, "bridged"));
    EXPECT_EQ(got, bridged ? "true" : "false");
}

INSTANTIATE_TEST_SUITE_P(getFetchesBridged,
                         TestBridgedInstanceSettings,
                         Values(std::make_pair("eth8", true),
                                std::make_pair("br-eth8", true),
                                std::make_pair("eth9", false),
                                std::make_pair("br-eth9", false)));

TEST_F(TestInstanceSettingsHandler, getFetchesPropertiesOfInstanceInSpecialState)
{
    constexpr auto preparing_instance = "nouvelle", deleted_instance = "vague";
    specs[preparing_instance];
    specs[deleted_instance];

    fake_instance_state(preparing_instance, SpecialInstanceState::preparing);
    fake_instance_state(deleted_instance, SpecialInstanceState::deleted);

    const auto handler = make_handler();

    for (const auto* instance : {preparing_instance, deleted_instance})
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

    const auto handler = make_handler();

    auto property_it = std::begin(properties);
    for (const auto& instance : instances)
    {
        assert(property_it != std::end(properties));
        handler.get(make_key(instance, *property_it++));
    }

    EXPECT_FALSE(fake_persister_called);
}

TEST_F(TestInstanceSettingsHandler, constOperationsDoNotModifyInstances) /* note that `const` on the respective methods
isn't enough for the compiler to catch changes to vms and specs, which live outside the handler (only references held
there) */
{
    constexpr auto make_mem_size = [](int gigs) { return mp::MemorySize{fmt::format("{}GiB", gigs)}; };
    auto gigs = 1;

    mp::VMSpecs spec;
    spec.num_cores = 3;
    spec.mem_size = make_mem_size(gigs);
    spec.ssh_username = "hugo";
    spec.default_mac_address = "+++++";

    for (const auto* name : {"toto", "tata", "fuzz"})
    {
        vms[name];
        specs.emplace(name, spec);
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

TEST_F(TestInstanceSettingsHandler, getThrowsOnWrongProperty)
{
    constexpr auto target_instance_name = "asdf";
    constexpr auto wrong_property = "wrong";
    specs[target_instance_name];

    MP_EXPECT_THROW_THAT(make_handler().get(make_key(target_instance_name, wrong_property)),
                         mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(wrong_property)));
}

TEST_F(TestInstanceSettingsHandler, setIncreasesInstanceCPUs)
{
    constexpr auto target_instance_name = "foo";
    constexpr auto more_cpus = 6;
    specs.insert({{"bar", {}}, {target_instance_name, {}}});
    const auto& actual_cpus = specs[target_instance_name].num_cores = 4;

    EXPECT_CALL(mock_vm(target_instance_name), update_cpus(more_cpus)).Times(1);

    make_handler().set(make_key(target_instance_name, "cpus"), QString::number(more_cpus));
    EXPECT_EQ(actual_cpus, more_cpus);
}

TEST_F(TestInstanceSettingsHandler, setMaintainsInstanceCPUsUntouchedIfSameButSucceeds)
{
    constexpr auto target_instance_name = "asdf";
    constexpr auto same_cpus = 42;
    const auto& actual_cpus = specs[target_instance_name].num_cores = same_cpus;

    EXPECT_CALL(mock_vm(target_instance_name), update_cpus).Times(0);

    EXPECT_NO_THROW(make_handler().set(make_key(target_instance_name, "cpus"), QString::number(same_cpus)));
    EXPECT_EQ(actual_cpus, same_cpus);
}

TEST_F(TestInstanceSettingsHandler, setAllowsDecreaseInstanceCPUs)
{
    constexpr auto target_instance_name = "sdfg";
    constexpr auto less_cpus = 2;
    const auto& actual_cpus = specs[target_instance_name].num_cores = 3;

    EXPECT_CALL(mock_vm(target_instance_name), update_cpus).Times(1);

    EXPECT_NO_THROW(make_handler().set(make_key(target_instance_name, "cpus"), QString::number(less_cpus)));
    EXPECT_EQ(actual_cpus, less_cpus);
}

TEST_F(TestInstanceSettingsHandler, setExpandsInstanceMemory)
{
    constexpr auto target_instance_name = "desmond";
    constexpr auto more_mem_str = "64G";
    const auto more_mem = mp::MemorySize{more_mem_str};
    const auto& actual_mem = specs[target_instance_name].mem_size = mp::MemorySize{"512M"};

    EXPECT_CALL(mock_vm(target_instance_name), resize_memory(Eq(more_mem))).Times(1);

    make_handler().set(make_key(target_instance_name, "memory"), more_mem_str);
    EXPECT_EQ(actual_mem, more_mem);
}

TEST_F(TestInstanceSettingsHandler, setMaintainsInstanceMemoryUntouchedIfSameButSucceeds)
{
    constexpr auto target_instance_name = "Liszt";
    constexpr auto same_mem_str = "1234567890";
    const auto same_mem = mp::MemorySize{same_mem_str};
    const auto& actual_mem = specs[target_instance_name].mem_size = same_mem;

    EXPECT_CALL(mock_vm(target_instance_name), resize_memory).Times(0);

    EXPECT_NO_THROW(make_handler().set(make_key(target_instance_name, "memory"), same_mem_str));
    EXPECT_EQ(actual_mem, same_mem);
}

TEST_F(TestInstanceSettingsHandler, setAllowsDecreaseInstanceMemory)
{
    constexpr auto target_instance_name = "Dvořák";
    constexpr auto less_mem_str = "256MiB";
    const auto less_mem = mp::MemorySize{less_mem_str};
    const auto& actual_mem = specs[target_instance_name].mem_size = mp::MemorySize{"1024MiB"};

    EXPECT_CALL(mock_vm(target_instance_name), resize_memory).Times(1);

    EXPECT_NO_THROW(make_handler().set(make_key(target_instance_name, "memory"), less_mem_str));
    EXPECT_EQ(actual_mem, less_mem);
}

TEST_F(TestInstanceSettingsHandler, setRefusesDecreaseBelowMinimumMemory)
{
    constexpr auto target_instance_name = "Ravel";
    constexpr auto mem_str = "96MiB";
    const auto& actual_mem = specs[target_instance_name].mem_size = mp::MemorySize{"1024MiB"};
    const auto original_mem = actual_mem;

    EXPECT_CALL(mock_vm(target_instance_name), resize_memory).Times(0);

    MP_EXPECT_THROW_THAT(make_handler().set(make_key(target_instance_name, "memory"), mem_str),
                         mp::InvalidSettingException, mpt::match_what(HasSubstr("minimum not allowed")));

    EXPECT_EQ(actual_mem, original_mem);
}

TEST_F(TestInstanceSettingsHandler, setExpandsInstanceDisk)
{
    constexpr auto target_instance_name = "Rimsky-Korsakov";
    constexpr auto more_disk_str = "20G";
    const auto more_disk = mp::MemorySize{more_disk_str};
    const auto& actual_disk = specs[target_instance_name].disk_space = mp::MemorySize{"5G"};

    EXPECT_CALL(mock_vm(target_instance_name), resize_disk(Eq(more_disk))).Times(1);

    make_handler().set(make_key(target_instance_name, "disk"), more_disk_str);
    EXPECT_EQ(actual_disk, more_disk);
}

TEST_F(TestInstanceSettingsHandler, setMaintainsInstanceDiskUntouchedIfSameButSucceeds)
{
    constexpr auto target_instance_name = "Gershwin";
    constexpr auto same_disk_str = "888999777K";
    const auto same_disk = mp::MemorySize{same_disk_str};
    const auto& actual_disk = specs[target_instance_name].disk_space = same_disk;

    EXPECT_CALL(mock_vm(target_instance_name), resize_disk).Times(0);

    EXPECT_NO_THROW(make_handler().set(make_key(target_instance_name, "disk"), same_disk_str));
    EXPECT_EQ(actual_disk, same_disk);
}

TEST_F(TestInstanceSettingsHandler, setResusesToShrinkInstanceDisk)
{
    constexpr auto target_instance_name = "Chopin";
    constexpr auto less_disk_str = "7Mb";
    const auto& actual_disk = specs[target_instance_name].disk_space = mp::MemorySize{"2Gb"};
    const auto original_disk = actual_disk;

    EXPECT_CALL(mock_vm(target_instance_name), resize_disk).Times(0);

    MP_EXPECT_THROW_THAT(make_handler().set(make_key(target_instance_name, "disk"), less_disk_str),
                         mp::InvalidSettingException, mpt::match_what(HasSubstr("can only be expanded")));

    EXPECT_EQ(actual_disk, original_disk);
}

TEST_F(TestInstanceSettingsHandler, setRefusesWrongProperty)
{
    constexpr auto target_instance_name = "desmond";
    constexpr auto wrong_property = "nuts";

    const auto original_specs = specs[target_instance_name];
    EXPECT_CALL(mock_vm(target_instance_name), update_cpus).Times(0);

    MP_EXPECT_THROW_THAT(make_handler().set(make_key(target_instance_name, wrong_property), "1"),
                         mp::UnrecognizedSettingException, mpt::match_what(HasSubstr(wrong_property)));

    EXPECT_EQ(original_specs, specs[target_instance_name]);
}

TEST_F(TestInstanceSettingsHandler, setRefusesToUnbridge)
{
    constexpr auto target_instance_name = "hendrix";
    specs.insert({{"voodoo", {}}, {"chile", {}}, {target_instance_name, {}}});
    specs[target_instance_name].extra_interfaces = {{"eth8", "52:54:00:78:90:12", true}};

    mock_vm(target_instance_name); // TODO: make this an expectation.

    MP_EXPECT_THROW_THAT(make_handler().set(make_key(target_instance_name, "bridged"), "false"),
                         mp::InvalidSettingException,
                         mpt::match_what(HasSubstr("not supported")));
}

TEST_F(TestInstanceSettingsHandler, setAddsInterface)
{
    constexpr auto target_instance_name = "pappo";
    specs.insert({{"blues", {}}, {"local", {}}, {target_instance_name, {}}});
    specs[target_instance_name].extra_interfaces = {{"id", "52:54:00:45:67:89", true}};

    mock_vm(target_instance_name); // TODO: make this an expectation.

    make_handler().set(make_key(target_instance_name, "bridged"), "true");

    EXPECT_EQ(specs[target_instance_name].extra_interfaces.size(), 2u);
    EXPECT_TRUE(mpu::valid_mac_address(specs[target_instance_name].extra_interfaces[1].mac_address));
}

TEST_F(TestInstanceSettingsHandler, setDoesNotAddTwoInterfaces)
{
    constexpr auto target_instance_name = "vitico";
    specs.insert({{"viticus", {}}, {"super", {}}, {target_instance_name, {}}});
    specs[target_instance_name].extra_interfaces = {{"br-eth8", "52:54:00:45:67:90", true}};

    mock_vm(target_instance_name); // TODO: make this an expectation.

    auto got = make_handler().get(make_key(target_instance_name, "bridged"));
    EXPECT_EQ(got, "true");

    make_handler().set(make_key(target_instance_name, "bridged"), "true");

    EXPECT_EQ(specs[target_instance_name].extra_interfaces.size(), 1u);
}

using VMSt = mp::VirtualMachine::State;
using Property = const char*;
using PropertyAndState = std::tuple<Property, VMSt>; // no subliminal political msg intended :)
struct TestInstanceModOnNonStoppedInstance : public TestInstanceSettingsHandler,
                                             public WithParamInterface<PropertyAndState>
{
};

TEST_P(TestInstanceModOnNonStoppedInstance, setRefusesToModifyNonStoppedInstances)
{
    constexpr auto target_instance_name = "Mozart";
    const auto [property, state] = GetParam();

    const auto original_specs = specs[target_instance_name];

    auto& target_instance = mock_vm<StrictMock>(target_instance_name);
    EXPECT_CALL(target_instance, current_state).WillOnce(Return(state));

    MP_EXPECT_THROW_THAT(make_handler().set(make_key(target_instance_name, property), "123"),
                         mp::InstanceStateSettingsException,
                         mpt::match_what(AllOf(HasSubstr("Cannot update"), HasSubstr("Instance must be stopped"))));

    EXPECT_EQ(original_specs, specs[target_instance_name]);
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsHandler, TestInstanceModOnNonStoppedInstance,
                         Combine(ValuesIn(TestInstanceSettingsHandler::properties),
                                 Values(VMSt::running, VMSt::restarting, VMSt::starting, VMSt::delayed_shutdown,
                                        VMSt::suspended, VMSt::suspending, VMSt::unknown)));

struct TestInstanceModOnStoppedInstance : public TestInstanceSettingsHandler,
                                          public WithParamInterface<PropertyAndState>
{
};

TEST_P(TestInstanceModOnStoppedInstance, setWorksOnOtherStates)
{
    constexpr auto target_instance_name = "Beethoven";
    constexpr auto val = "134217728"; // To work with the 128M minimum allowed for memory
    const auto [property, state] = GetParam();
    const auto& target_specs = specs[target_instance_name];

    EXPECT_CALL(mock_vm(target_instance_name), current_state).WillOnce(Return(state));

    EXPECT_NO_THROW(make_handler().set(make_key(target_instance_name, property), val));

    const auto props = {static_cast<long long>(target_specs.num_cores), target_specs.mem_size.in_bytes(),
                        target_specs.disk_space.in_bytes()};
    EXPECT_THAT(props, Contains(QString{val}.toLongLong()));
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsHandler,
                         TestInstanceModOnStoppedInstance,
                         Combine(ValuesIn(TestInstanceSettingsHandler::numeric_properties),
                                 Values(VMSt::off, VMSt::stopped)));

struct TestInstanceModPersists : public TestInstanceSettingsHandler, public WithParamInterface<Property>
{
};

TEST_P(TestInstanceModPersists, setPersistsInstances)
{
    constexpr auto target_instance_name = "Tchaikovsky";
    constexpr auto val = "134217728"; // To work with the 128M minimum allowed for memory
    const auto property = GetParam();

    specs[target_instance_name];
    mock_vm(target_instance_name);

    make_handler().set(make_key(target_instance_name, property), val);
    EXPECT_TRUE(fake_persister_called);
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsHandler,
                         TestInstanceModPersists,
                         ValuesIn(TestInstanceSettingsHandler::numeric_properties));

TEST_F(TestInstanceSettingsHandler, setRefusesToModifyInstancesInSpecialState)
{
    constexpr auto preparing_instance_name = "Yann", deleted_instance_name = "Tiersen";
    specs[preparing_instance_name];
    specs[deleted_instance_name];
    const auto original_specs = specs;

    fake_instance_state(preparing_instance_name, SpecialInstanceState::preparing);
    auto& preparing_instance = mock_vm(preparing_instance_name);
    auto& deleted_instance = mock_vm(deleted_instance_name, /*deleted=*/true);

    auto handler = make_handler();

    for (const auto& [instance, instance_name, special_state] :
         {std::tuple{&preparing_instance, preparing_instance_name, "prepared"},
          std::tuple{&deleted_instance, deleted_instance_name, "deleted"}})
    {
        EXPECT_CALL(*instance, update_cpus).Times(0);
        EXPECT_CALL(*instance, resize_memory).Times(0);
        EXPECT_CALL(*instance, resize_disk).Times(0);
        for (const auto& property : properties)
        {
            MP_EXPECT_THROW_THAT(handler.set(make_key(instance_name, property), "234"), mp::InstanceSettingsException,
                                 mpt::match_what(AllOf(HasSubstr("Cannot update"), HasSubstr(special_state))));
        }
    }

    EXPECT_EQ(specs, original_specs);
}

TEST_F(TestInstanceSettingsHandler, getAndSetThrowOnMissingInstance)
{
    constexpr auto instance = "missing-instance";

    auto handler = make_handler();

    for (const auto& prop : properties)
    {
        MP_EXPECT_THROW_THAT(handler.get(make_key(instance, prop)), mp::InstanceSettingsException,
                             mpt::match_what(AllOf(HasSubstr(instance), HasSubstr("No such instance"))));
        MP_EXPECT_THROW_THAT(handler.set(make_key(instance, prop), "1"), mp::InstanceSettingsException,
                             mpt::match_what(AllOf(HasSubstr(instance), HasSubstr("No such instance"))));
    }
}

TEST_F(TestInstanceSettingsHandler, getAndSetThrowOnBadKey)
{
    constexpr auto bad_key = ".#^&nonsense-.-$-$";
    MP_EXPECT_THROW_THAT(make_handler().get(bad_key), mp::UnrecognizedSettingException,
                         mpt::match_what(HasSubstr(bad_key)));
    MP_EXPECT_THROW_THAT(make_handler().set(bad_key, "1"), mp::UnrecognizedSettingException,
                         mpt::match_what(HasSubstr(bad_key)));
}

using PlainValue = const char*;
using PropertyValue = std::tuple<Property, PlainValue>;
struct TestInstanceSettingsHandlerBadNumericValues : public TestInstanceSettingsHandler,
                                                     public WithParamInterface<PropertyValue>
{
};

TEST_P(TestInstanceSettingsHandlerBadNumericValues, setRefusesBadNumericValues)
{
    constexpr auto target_instance_name = "xanana";
    const auto& [property, bad_val] = GetParam();

    const auto original_specs = specs[target_instance_name];
    EXPECT_CALL(mock_vm(target_instance_name), update_cpus).Times(0);

    MP_EXPECT_THROW_THAT(
        make_handler().set(make_key(target_instance_name, property), bad_val), mp::InvalidSettingException,
        mpt::match_what(AllOf(HasSubstr(bad_val), AnyOf(HasSubstr("positive"), HasSubstr("non-negative")))));

    EXPECT_EQ(original_specs, specs[target_instance_name]);
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsHandler,
                         TestInstanceSettingsHandlerBadNumericValues,
                         Combine(ValuesIn(TestInstanceSettingsHandler::numeric_properties),
                                 Values("0",
                                        "2u",
                                        "1.5f",
                                        "2.0",
                                        "0xa",
                                        "0x8",
                                        "-4",
                                        "-1",
                                        "rubbish",
                                        "  123nonsense ",
                                        "¤9",
                                        "\n",
                                        "\t",
                                        "^",
                                        "")));

struct TestInstanceSettingsHandlerBadBooleanValues : public TestInstanceSettingsHandler,
                                                     public WithParamInterface<PropertyValue>
{
};

TEST_P(TestInstanceSettingsHandlerBadBooleanValues, setRefusesBadBooleanValues)
{
    constexpr auto target_instance_name = "zappa";
    const auto& [property, bad_val] = GetParam();

    const auto original_specs = specs[target_instance_name];
    mock_vm(target_instance_name); // TODO: make this an expectation.

    MP_EXPECT_THROW_THAT(make_handler().set(make_key(target_instance_name, property), bad_val),
                         mp::InvalidSettingException,
                         mpt::match_what(AllOf(HasSubstr(bad_val), HasSubstr("try \"true\" or \"false\""))));

    EXPECT_EQ(original_specs, specs[target_instance_name]);
}

INSTANTIATE_TEST_SUITE_P(TestInstanceSettingsHandler,
                         TestInstanceSettingsHandlerBadBooleanValues,
                         Combine(ValuesIn(TestInstanceSettingsHandler::boolean_properties),
                                 Values("apostrophe", "(')", "1974")));
} // namespace
