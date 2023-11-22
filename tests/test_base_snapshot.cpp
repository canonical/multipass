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
#include "file_operations.h"
#include "mock_virtual_machine.h"
#include "path.h"

#include <multipass/memory_size.h>
#include <multipass/vm_specs.h>
#include <shared/base_snapshot.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <stdexcept>
#include <tuple>

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

    friend bool operator==(const MockBaseSnapshot& a, const MockBaseSnapshot& b);
};

bool operator==(const MockBaseSnapshot& a, const MockBaseSnapshot& b)
{
    return std::tuple(a.get_index(),
                      a.get_name(),
                      a.get_comment(),
                      a.get_creation_timestamp(),
                      a.get_num_cores(),
                      a.get_mem_size(),
                      a.get_disk_space(),
                      a.get_state(),
                      a.get_mounts(),
                      a.get_metadata(),
                      a.get_parent(),
                      a.get_id()) == std::tuple(b.get_index(),
                                                b.get_name(),
                                                b.get_comment(),
                                                b.get_creation_timestamp(),
                                                b.get_num_cores(),
                                                b.get_mem_size(),
                                                b.get_disk_space(),
                                                b.get_state(),
                                                b.get_mounts(),
                                                b.get_metadata(),
                                                b.get_parent(),
                                                b.get_id());
}

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

    static QJsonObject test_snapshot_json()
    {
        static auto json_doc = [] {
            QJsonParseError parse_error{};
            const auto ret = QJsonDocument::fromJson(mpt::load_test_file(test_json_filename), &parse_error);
            if (parse_error.error)
                throw std::runtime_error{
                    fmt::format("Bad JSON test data in {}; error: {}", test_json_filename, parse_error.errorString())};
            return ret;
        }();

        return json_doc.object();
    }

    static void mod_snapshot_json(QJsonObject& json, const QString& key, const QJsonValue& new_value)
    {
        const auto snapshot_key = QStringLiteral("snapshot");
        auto snapshot_json_ref = json[snapshot_key];
        auto snapshot_json_copy = snapshot_json_ref.toObject();
        snapshot_json_copy[key] = new_value;
        snapshot_json_ref = snapshot_json_copy;
    }

    QString plant_snapshot_json(const QJsonObject& object, const QString& filename = "snapshot.json") const
    {
        const auto file_path = vm.tmp_dir->filePath(filename);

        const QJsonDocument doc{object};
        mpt::make_file_with_content(file_path, doc.toJson().toStdString());

        return file_path;
    }

    QString derive_persisted_snapshot_filename(int index)
    {
        return vm.tmp_dir->filePath(QString{"%1"}.arg(index, 4, 10, QLatin1Char('0')) + ".snapshot.json");
    }

    static constexpr auto* test_json_filename = "test_snapshot.json";
    mp::VMSpecs specs = stub_specs();
    mpt::MockVirtualMachine vm{"a-vm"}; // TODO@no-merge nice?
};

TEST_F(TestBaseSnapshot, adoptsGivenValidName)
{
    constexpr auto name = "a-name";
    auto snapshot = MockBaseSnapshot{name, "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_name(), name);
}

TEST_F(TestBaseSnapshot, rejectsEmptyName)
{
    const std::string empty{};
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{empty, "asdf", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("empty")));
}

TEST_F(TestBaseSnapshot, adoptsGivenComment)
{
    constexpr auto comment = "some comment";
    auto snapshot = MockBaseSnapshot{"whatever", comment, nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_comment(), comment);
}

TEST_F(TestBaseSnapshot, adoptsGivenParent)
{
    const auto parent = std::make_shared<MockBaseSnapshot>("root", "asdf", nullptr, specs, vm);
    auto snapshot = MockBaseSnapshot{"descendant", "descends", parent, specs, vm};
    EXPECT_EQ(snapshot.get_parent(), parent);
}

TEST_F(TestBaseSnapshot, adoptsNullParent)
{
    auto snapshot = MockBaseSnapshot{"descendant", "descends", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_parent(), nullptr);
}

TEST_F(TestBaseSnapshot, adoptsGivenSpecs)
{
    auto snapshot = MockBaseSnapshot{"snapshot", "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_num_cores(), specs.num_cores);
    EXPECT_EQ(snapshot.get_mem_size(), specs.mem_size);
    EXPECT_EQ(snapshot.get_disk_space(), specs.disk_space);
    EXPECT_EQ(snapshot.get_state(), specs.state);
    EXPECT_EQ(snapshot.get_mounts(), specs.mounts);
    EXPECT_EQ(snapshot.get_metadata(), specs.metadata);
}

TEST_F(TestBaseSnapshot, adoptsCustomMounts)
{
    specs.mounts["toto"] =
        mp::VMMount{"src", {{123, 234}, {567, 678}}, {{19, 91}}, multipass::VMMount::MountType::Classic};
    specs.mounts["tata"] =
        mp::VMMount{"fountain", {{234, 123}}, {{81, 18}, {9, 10}}, multipass::VMMount::MountType::Native};

    auto snapshot = MockBaseSnapshot{"snapshot", "", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_mounts(), specs.mounts);
}

TEST_F(TestBaseSnapshot, adoptsCustomMetadata)
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

TEST_F(TestBaseSnapshot, adoptsNextIndex)
{
    const int count = 123;
    EXPECT_CALL(vm, get_snapshot_count).WillOnce(Return(count));

    auto snapshot = MockBaseSnapshot{"tau", "ceti", nullptr, specs, vm};
    EXPECT_EQ(snapshot.get_index(), count + 1);
}

TEST_F(TestBaseSnapshot, retrievesParentsProperties)
{
    constexpr auto parent_name = "parent";
    const int parent_index = 11;

    EXPECT_CALL(vm, get_snapshot_count).WillOnce(Return(parent_index - 1)).WillOnce(Return(31));

    auto parent = std::make_shared<MockBaseSnapshot>(parent_name, "", nullptr, specs, vm);

    auto child = MockBaseSnapshot{"child", "", parent, specs, vm};
    EXPECT_EQ(child.get_parents_index(), parent_index);
    EXPECT_EQ(child.get_parents_name(), parent_name);
}

TEST_F(TestBaseSnapshot, adoptsCurrentTimestamp)
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

TEST_P(TestSnapshotRejectedStates, rejectsActiveState)
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

TEST_P(TestSnapshotInvalidCores, rejectsInvalidNumberOfCores)
{
    specs.num_cores = GetParam();
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Invalid number of cores")));
}

INSTANTIATE_TEST_SUITE_P(TestBaseSnapshot, TestSnapshotInvalidCores, Values(0, -1, -12345, -3e9));

TEST_F(TestBaseSnapshot, rejectsNullMemorySize)
{
    specs.mem_size = mp::MemorySize{"0B"};
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Invalid memory size")));
}

TEST_F(TestBaseSnapshot, rejectsNullDiskSize)
{
    specs.disk_space = mp::MemorySize{"0B"};
    MP_EXPECT_THROW_THAT((MockBaseSnapshot{"snapshot", "comment", nullptr, specs, vm}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("Invalid disk size")));
}

TEST_F(TestBaseSnapshot, reconstructsFromJson)
{
    MockBaseSnapshot{multipass::test::test_data_path_for(test_json_filename), vm};
}

TEST_F(TestBaseSnapshot, adoptsNameFromJson)
{
    constexpr auto* snapshot_name = "cheeseball";
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "name", snapshot_name);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_name(), snapshot_name);
}

TEST_F(TestBaseSnapshot, adoptsCommentFromJson)
{
    constexpr auto* snapshot_comment = "Look behind you, a three-headed monkey!";
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "comment", snapshot_comment);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_comment(), snapshot_comment);
}

TEST_F(TestBaseSnapshot, linksToParentFromJson)
{
    constexpr auto parent_idx = 42;
    constexpr auto parent_name = "s42";
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "parent", parent_idx);

    EXPECT_CALL(vm, get_snapshot(TypedEq<int>(parent_idx)))
        .WillOnce(Return(std::make_shared<MockBaseSnapshot>(parent_name, "mock parent snapshot", nullptr, specs, vm)));

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_parents_name(), parent_name);
}

TEST_F(TestBaseSnapshot, adoptsIndexFromJson)
{
    constexpr auto index = 31;
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "index", index);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_index(), index);
}

TEST_F(TestBaseSnapshot, adoptsTimestampFromJson)
{
    constexpr auto timestamp = "1990-10-01T01:02:03.999Z";
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "creation_timestamp", timestamp);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_creation_timestamp().toString(Qt::ISODateWithMs), timestamp);
}

TEST_F(TestBaseSnapshot, adoptsNumCoresFromJson)
{
    constexpr auto num_cores = 9;
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "num_cores", num_cores);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_num_cores(), num_cores);
}

TEST_F(TestBaseSnapshot, adoptsMemSizeFromJson)
{
    constexpr auto mem = "1073741824";
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "mem_size", mem);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_mem_size().in_bytes(), QString{mem}.toInt());
}

TEST_F(TestBaseSnapshot, adoptsDiskSpaceFromJson)
{
    constexpr auto disk = "1073741824";
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "disk_space", disk);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_disk_space().in_bytes(), QString{disk}.toInt());
}

TEST_F(TestBaseSnapshot, adoptsStateFromJson)
{
    constexpr auto state = mp::VirtualMachine::State::stopped;
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "state", static_cast<int>(state));

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_state(), state);
}

TEST_F(TestBaseSnapshot, adoptsMetadataFromJson)
{
    auto metadata = QJsonObject{};
    metadata["arguments"] = "Meathook:\n"
                            "You've got a real attitude problem!\n"
                            "\n"
                            "Guybrush Threepwood:\n"
                            "Well... you've got a real hair problem!\n"
                            "\n"
                            "Meathook:\n"
                            "You just don't know when to quit, do you?\n"
                            "\n"
                            "Guybrush Threepwood:\n"
                            "Neither did your barber.";

    auto json = test_snapshot_json();
    mod_snapshot_json(json, "metadata", metadata);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    EXPECT_EQ(snapshot.get_metadata(), metadata);
}

TEST_F(TestBaseSnapshot, adoptsMountsFromJson)
{
    constexpr auto src_path = "You fight like a dairy farmer.";
    constexpr auto dst_path = "How appropriate. You fight like a cow.";
    constexpr auto host_uid = 1, instance_uid = 2, host_gid = 3, instance_gid = 4;
    constexpr auto mount_type = mp::VMMount::MountType::Native;

    QJsonArray mounts{};
    QJsonObject mount{};
    QJsonArray uid_mappings{};
    QJsonObject uid_mapping{};
    QJsonArray gid_mappings{};
    QJsonObject gid_mapping{};

    uid_mapping["host_uid"] = host_uid;
    uid_mapping["instance_uid"] = instance_uid;
    uid_mappings.append(uid_mapping);

    gid_mapping["host_gid"] = host_gid;
    gid_mapping["instance_gid"] = instance_gid;
    gid_mappings.append(gid_mapping);

    mount["source_path"] = src_path;
    mount["target_path"] = dst_path;
    mount["uid_mappings"] = uid_mappings;
    mount["gid_mappings"] = gid_mappings;
    mount["mount_type"] = static_cast<int>(mount_type);

    mounts.append(mount);

    auto json = test_snapshot_json();
    mod_snapshot_json(json, "mounts", mounts);

    auto snapshot = MockBaseSnapshot{plant_snapshot_json(json), vm};
    auto snapshot_mounts = snapshot.get_mounts();

    ASSERT_THAT(snapshot_mounts, SizeIs(mounts.size()));
    const auto [snapshot_mnt_dst, snapshot_mount] = *snapshot_mounts.begin();

    EXPECT_EQ(snapshot_mnt_dst, dst_path);
    EXPECT_EQ(snapshot_mount.source_path, src_path);
    EXPECT_EQ(snapshot_mount.mount_type, mount_type);

    ASSERT_THAT(snapshot_mount.uid_mappings, SizeIs(uid_mappings.size()));
    const auto [snapshot_host_uid, snapshot_instance_uid] = snapshot_mount.uid_mappings.front();

    EXPECT_EQ(snapshot_host_uid, host_uid);
    EXPECT_EQ(snapshot_instance_uid, instance_uid);

    ASSERT_THAT(snapshot_mount.gid_mappings, SizeIs(gid_mappings.size()));
    const auto [snapshot_host_gid, snapshot_instance_gid] = snapshot_mount.gid_mappings.front();

    EXPECT_EQ(snapshot_host_gid, host_gid);
    EXPECT_EQ(snapshot_instance_gid, instance_gid);
}

class TestSnapshotRejectedNonPositiveIndices : public TestBaseSnapshot, public WithParamInterface<int>
{
};

TEST_P(TestSnapshotRejectedNonPositiveIndices, refusesNonPositiveIndexFromJson)
{
    const auto index = GetParam();
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "index", index);

    MP_EXPECT_THROW_THAT((MockBaseSnapshot{plant_snapshot_json(json), vm}),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("not positive"), HasSubstr(std::to_string(index)))));
}

INSTANTIATE_TEST_SUITE_P(TestBaseSnapshot, TestSnapshotRejectedNonPositiveIndices, Values(0, -1, -31));

TEST_F(TestBaseSnapshot, refusesIndexAboveMax)
{
    constexpr auto index = 25623956;
    auto json = test_snapshot_json();
    mod_snapshot_json(json, "index", index);

    MP_EXPECT_THROW_THAT((MockBaseSnapshot{plant_snapshot_json(json), vm}),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Maximum"), HasSubstr(std::to_string(index)))));
}

class TestSnapshotPersistence : public TestBaseSnapshot,
                                public WithParamInterface<std::function<void(MockBaseSnapshot&)>>
{
};

TEST_P(TestSnapshotPersistence, persistsOnEdition)
{
    constexpr auto index = 55;
    auto setter = GetParam();

    auto json = test_snapshot_json();
    mod_snapshot_json(json, "index", index);

    MockBaseSnapshot snapshot_orig{plant_snapshot_json(json), vm};
    setter(snapshot_orig);

    const auto filename = derive_persisted_snapshot_filename(index);
    const MockBaseSnapshot snapshot_edited{filename, vm};
    EXPECT_EQ(snapshot_edited, snapshot_orig);
}

INSTANTIATE_TEST_SUITE_P(TestBaseSnapshot,
                         TestSnapshotPersistence,
                         Values([](MockBaseSnapshot& s) { s.set_name("asdf"); },
                                [](MockBaseSnapshot& s) { s.set_comment("fdsa"); },
                                [](MockBaseSnapshot& s) { s.set_parent(nullptr); }));

} // namespace
