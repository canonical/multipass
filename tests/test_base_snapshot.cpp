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

#include "multipass/vm_specs.h"
#include "shared/base_snapshot.h"

#include <multipass/memory_size.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
class MockBaseSnapshot : public mp::BaseSnapshot
{
public:
    using mp::BaseSnapshot::BaseSnapshot;

    MOCK_METHOD(void, capture_impl, (), (override));
    MOCK_METHOD(void, erase_impl, (), (override));
    MOCK_METHOD(void, apply_impl, (), (override));
};

struct TestBaseSnapshot : public Test
{
    static mp::VMSpecs stub_specs()
    {
        mp::VMSpecs ret{};
        ret.num_cores = 3;
        ret.mem_size = mp::MemorySize{"1.5G"};
        ret.disk_space = mp::MemorySize{"10G"};
        ret.default_mac_address = "12:12:12:12:12:12";

        return ret;
    }

    mp::VMSpecs specs = stub_specs();
    mpt::MockVirtualMachine vm{"a-vm"}; // TODO@no-merge nice?
};

TEST_F(TestBaseSnapshot, adopts_given_valid_name)
{
    auto name = "a-name";
    auto snapshot = MockBaseSnapshot{name, "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_name(), name);
}

TEST_F(TestBaseSnapshot, rejects_empty_name)
{
    std::string empty{};
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{empty, "asdf", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("empty")));
}

TEST_F(TestBaseSnapshot, adopts_given_comment)
{
    auto comment = "some comment";
    auto snapshot = MockBaseSnapshot{"whatever", comment, nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_comment(), comment);
}

TEST_F(TestBaseSnapshot, adopts_given_parent)
{
    auto parent = std::make_shared<MockBaseSnapshot>("root", "asdf", nullptr, specs, vm);
    auto snapshot = MockBaseSnapshot{"descendant", "descends", parent, specs, vm};
    EXPECT_EQ(snapshot.get_parent(), parent);
}

TEST_F(TestBaseSnapshot, adopts_null_parent)
{
    auto snapshot = MockBaseSnapshot{"descendant", "descends", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_parent(), nullptr);
}

TEST_F(TestBaseSnapshot, adopts_given_specs)
{
    auto snapshot = MockBaseSnapshot{"snapshot", "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_num_cores(), specs.num_cores);
    EXPECT_EQ(snapshot.get_mem_size(), specs.mem_size);
    EXPECT_EQ(snapshot.get_disk_space(), specs.disk_space);
    EXPECT_EQ(snapshot.get_state(), specs.state);
    EXPECT_EQ(snapshot.get_mounts(), specs.mounts);
    EXPECT_EQ(snapshot.get_metadata(), specs.metadata);
}

TEST_F(TestBaseSnapshot, adopts_custom_mounts)
{
    specs.mounts["toto"] =
        mp::VMMount{"src", {{123, 234}, {567, 678}}, {{19, 91}}, multipass::VMMount::MountType::Classic};
    specs.mounts["tata"] =
        mp::VMMount{"fountain", {{234, 123}}, {{81, 18}, {9, 10}}, multipass::VMMount::MountType::Native};

    auto snapshot = MockBaseSnapshot{"snapshot", "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_mounts(), specs.mounts);
}

TEST_F(TestBaseSnapshot, adopts_custom_metadata)
{
    QJsonObject json;
    QJsonObject data;
    data.insert("an-int", 7);
    data.insert("a-str", "str");
    json.insert("meta", data);
    specs.metadata = json;

    auto snapshot = MockBaseSnapshot{"snapshot", "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_metadata(), specs.metadata);
}

TEST_F(TestBaseSnapshot, adopts_next_index)
{
    int count = 123;
    EXPECT_CALL(vm, get_snapshot_count).WillOnce(Return(count));

    auto snapshot = MockBaseSnapshot{"tau", "ceti", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_index(), count + 1);
}

TEST_F(TestBaseSnapshot, retrieves_parents_properties)
{
    int parent_index = 11;
    std::string parent_name = "parent";

    EXPECT_CALL(vm, get_snapshot_count).WillOnce(Return(parent_index - 1)).WillOnce(Return(31));

    auto parent = std::make_shared<MockBaseSnapshot>(parent_name, "", nullptr, specs, vm);

    auto child = MockBaseSnapshot{"child", "", parent, specs, vm};
    EXPECT_EQ(child.get_parents_index(), parent_index);
    EXPECT_EQ(child.get_parents_name(), parent_name);
}

TEST_F(TestBaseSnapshot, adopts_current_timestamp)
{
    auto before = QDateTime::currentDateTimeUtc();
    auto snapshot = MockBaseSnapshot{"foo", "", nullptr, specs, vm};
    auto after = QDateTime::currentDateTimeUtc();

    EXPECT_GE(snapshot.get_creation_timestamp(), before);
    EXPECT_LE(snapshot.get_creation_timestamp(), after);
}

class TestSnapshotRejectedStates : public TestBaseSnapshot, public WithParamInterface<mp::VirtualMachine::State>
{
};

TEST_P(TestSnapshotRejectedStates, rejects_active_state)
{
    specs.state = GetParam();
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Unsupported VM state")));
}

INSTANTIATE_TEST_SUITE_P(TestBaseSnapshot,
                         TestSnapshotRejectedStates,
                         Values(mp::VirtualMachine::State::starting,
                                mp::VirtualMachine::State::restarting,
                                mp::VirtualMachine::State::running,
                                mp::VirtualMachine::State::delayed_shutdown,
                                mp::VirtualMachine::State::suspending,
                                mp::VirtualMachine::State::suspended,
                                mp::VirtualMachine::State::unknown));

class TestSnapshotInvalidCores : public TestBaseSnapshot, public WithParamInterface<int>
{
};

TEST_P(TestSnapshotInvalidCores, rejects_invalid_number_of_cores)
{
    specs.num_cores = GetParam();
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Invalid number of cores")));
}

INSTANTIATE_TEST_SUITE_P(TestBaseSnapshot, TestSnapshotInvalidCores, Values(0, -1, -12345, -3e9));

TEST_F(TestBaseSnapshot, rejects_null_memory_size)
{
    specs.mem_size = mp::MemorySize{"0B"};
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Invalid memory size")));
}

TEST_F(TestBaseSnapshot, rejects_null_disk_size)
{
    specs.disk_space = mp::MemorySize{"0B"};
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Invalid disk size")));
}

} // namespace
