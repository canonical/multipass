/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tests/unit/common.h"
#include "tests/unit/file_operations.h"
#include "tests/unit/hyperv_api/mock_hyperv_virtdisk_wrapper.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/temp_dir.h"

#include <src/platform/backends/hyperv/hyperv_target_transaction.h>

#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <fstream>
#include <limits>
#include <map>
#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
namespace fs = std::filesystem;
using namespace testing;

namespace
{
// Fake VirtDisk chain resolver keyed by filename, so that both source and target copies
// (which share filenames) resolve to a parent living in the same directory as the queried
// disk. This lets a single expectation serve the plan (source) and verify (target) phases.
auto chain_by_filename(std::map<std::string, std::string> parents)
{
    return [parents = std::move(parents)](const fs::path& disk,
                                          std::vector<fs::path>& chain,
                                          std::optional<std::size_t>) {
        chain.clear();
        chain.push_back(disk);
        if (const auto it = parents.find(disk.filename().string()); it != parents.end())
            chain.push_back(disk.parent_path() / it->second);
        return mhv::OperationResult::success();
    };
}

void write_snapshot_json(const fs::path& path, int index, const fs::path& disk_path)
{
    const boost::json::object snapshot{
        {"name", fmt::format("snapshot{}", index)},
        {"comment", ""},
        {"parent", 0},
        {"cloud_init_instance_id", "instance-id"},
        {"index", index},
        {"disk_path", disk_path.string()},
        {"creation_timestamp", "2026-08-19T00:00:00.000Z"},
        {"num_cores", 1},
        {"mem_size", "1073741824"},
        {"disk_space", "5368709120"},
        {"extra_interfaces", boost::json::array{}},
        {"state", static_cast<int>(mp::VirtualMachine::State::stopped)},
        {"mounts", boost::json::array{}},
        {"metadata", boost::json::object{}}};
    MP_FILEOPS.write_transactionally(path,
                                     mp::pretty_print(boost::json::object{{"snapshot", snapshot}}));
}

struct HyperVTargetTransaction : public Test
{
    HyperVTargetTransaction()
    {
        source_dir = fs::path{data.path().toStdString()} / "source";
        instances_dir = fs::path{data.path().toStdString()} / "instances";
        target_instance_dir = instances_dir / vm_name;
        fs::create_directories(source_dir);
        fs::create_directories(instances_dir);

        base = source_dir / "base.vhdx";
        active = source_dir / "active.avhdx";
        snap1 = source_dir / "snap1.avhdx";
        mpt::make_file_with_content(QString::fromStdString(base.string()), "base-image");
        mpt::make_file_with_content(QString::fromStdString(active.string()), "active-head");
        mpt::make_file_with_content(QString::fromStdString(snap1.string()), "snapshot-one");

        snapshot_json = source_dir / "0001.snapshot.json";
        write_snapshot_json(snapshot_json, 1, snap1);
        mpt::make_file_with_content(QString::fromStdString((source_dir / "snapshot-head").string()),
                                    "0\n");
        mpt::make_file_with_content(
            QString::fromStdString((source_dir / "snapshot-count").string()),
            "1\n");
        mpt::make_file_with_content(
            QString::fromStdString((source_dir / mp::cloud_init_file_name).string()),
            "cloud-init");

        layout.active_disk = active;
        layout.all_disks = {active, base, snap1};
        layout.snapshots = {
            {.index = 1,
             .checkpoint_name = "@s1",
             .checkpoint_id = "checkpoint-id",
             .disk_path = snap1,
             .extra_interfaces = {
                 {.id = "target-switch", .mac_address = "52:54:00:12:34:56", .auto_mode = true}}}};

        ON_CALL(*virtdisk.first, list_virtual_disk_chain(_, _, _))
            .WillByDefault(Invoke(
                chain_by_filename({{"active.avhdx", "base.vhdx"}, {"snap1.avhdx", "base.vhdx"}})));
        ON_CALL(*virtdisk.first, reparent_virtual_disk(_, _))
            .WillByDefault(Return(mhv::OperationResult::success()));
    }

    mpt::TempDir data;
    const std::string vm_name{"migration-test"};
    fs::path source_dir;
    fs::path instances_dir;
    fs::path target_instance_dir;
    fs::path base, active, snap1, snapshot_json;
    mhv::LegacyDiskLayout layout;
    decltype(mpt::MockVirtDiskWrapper::inject<NiceMock>()) virtdisk{
        mpt::MockVirtDiskWrapper::inject<NiceMock>()};
};
} // namespace

TEST_F(HyperVTargetTransaction, planMapsEveryUniqueDiskTargetLocal)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    const auto mapping = transaction.plan(layout, target_instance_dir);

    ASSERT_EQ(mapping.disks.size(), 3u);
    for (const auto& entry : mapping.disks)
        EXPECT_EQ(entry.target.parent_path(), target_instance_dir);
    EXPECT_EQ(mapping.active_disk, target_instance_dir / "active.avhdx");
    ASSERT_EQ(mapping.snapshots.size(), 1u);
    EXPECT_EQ(mapping.snapshots.front().disk_path, target_instance_dir / "snap1.avhdx");

    ASSERT_EQ(mapping.parent_links.size(), 2u);
    for (const auto& link : mapping.parent_links)
        EXPECT_EQ(link.parent, target_instance_dir / "base.vhdx");
}

TEST_F(HyperVTargetTransaction, planRejectsSharedBackingOutsideGraph)
{
    ON_CALL(*virtdisk.first, list_virtual_disk_chain(_, _, _))
        .WillByDefault(Invoke(chain_by_filename({{"active.avhdx", "foreign-base.vhdx"}})));

    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    EXPECT_THROW((void)transaction.plan(layout, target_instance_dir), std::runtime_error);
}

TEST_F(HyperVTargetTransaction, stageCopiesReparentsAndRewritesSnapshotsWithoutTouchingSource)
{
    const auto original_snapshot = *MP_FILEOPS.try_read_file(snapshot_json);
    const auto original_active = *MP_FILEOPS.try_read_file(active);

    // Every reparent must target a staged copy, never the source.
    EXPECT_CALL(*virtdisk.first, reparent_virtual_disk(_, _))
        .Times(2)
        .WillRepeatedly(Invoke([&](const fs::path& child, const fs::path& parent) {
            EXPECT_NE(child.parent_path(), source_dir);
            EXPECT_NE(parent.parent_path(), source_dir);
            return mhv::OperationResult::success();
        }));

    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    const auto mapping = transaction.stage(layout, source_dir);
    const auto staging = transaction.staging_dir();

    // Target-local copies exist and match the source lengths.
    for (const auto& entry : mapping.disks)
    {
        ASSERT_TRUE(MP_FILEOPS.exists(entry.target));
        std::error_code ec;
        EXPECT_EQ(MP_FILEOPS.file_size(entry.target, ec), MP_FILEOPS.file_size(entry.source, ec));
    }

    // Snapshot bookkeeping is copied and rewritten to point at the target copy.
    const auto copied = MP_FILEOPS.try_read_file(staging / "0001.snapshot.json");
    ASSERT_TRUE(copied);
    const auto json = boost::json::parse(*copied);
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("snapshot").at("disk_path")),
              (staging / "snap1.avhdx").string());
    EXPECT_EQ(boost::json::value_to<std::vector<mp::NetworkInterface>>(
                  json.at("snapshot").at("extra_interfaces")),
              layout.snapshots.front().extra_interfaces);
    EXPECT_TRUE(MP_FILEOPS.exists(staging / "snapshot-head"));
    const auto snapshot_count = MP_FILEOPS.try_read_file(staging / "snapshot-count");
    const auto cloud_init = MP_FILEOPS.try_read_file(staging / mp::cloud_init_file_name);
    ASSERT_TRUE(snapshot_count);
    ASSERT_TRUE(cloud_init);
    EXPECT_EQ(*snapshot_count, "1\n");
    EXPECT_EQ(*cloud_init, "cloud-init");

    // Source snapshot metadata and disks are byte-for-byte unchanged.
    EXPECT_EQ(*MP_FILEOPS.try_read_file(snapshot_json), original_snapshot);
    EXPECT_EQ(*MP_FILEOPS.try_read_file(active), original_active);
    EXPECT_FALSE(MP_FILEOPS.exists(source_dir / "migration-transaction.json"));

    EXPECT_NO_THROW(transaction.verify(mapping, layout));
}

TEST_F(HyperVTargetTransaction, stagePersistsVersionedManifest)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    (void)transaction.stage(layout, source_dir);

    const auto manifest = mhv::MigrationTransactionManifest::load(transaction.staging_dir());
    ASSERT_TRUE(manifest);
    EXPECT_EQ(manifest->version, mhv::MigrationTransactionManifest::current_version);
    EXPECT_FALSE(manifest->transaction_id.empty());
    EXPECT_EQ(manifest->phase, mhv::MigrationTransactionManifest::staged_phase_name);
    EXPECT_EQ(manifest->vm_name, vm_name);
}

TEST_F(HyperVTargetTransaction, verifyRejectsMismatchedTargetLength)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    const auto mapping = transaction.stage(layout, source_dir);

    // Grow one of the target copies so its length no longer matches the source.
    {
        std::ofstream out{mapping.active_disk, std::ios::binary | std::ios::app};
        out << "extra-bytes-that-change-the-length";
    }

    EXPECT_THROW(transaction.verify(mapping, layout), std::runtime_error);
}

TEST_F(HyperVTargetTransaction, verifyRejectsNonTargetLocalParent)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    const auto mapping = transaction.stage(layout, source_dir);

    // Make the active head reopen onto a parent that lives outside the target root.
    ON_CALL(*virtdisk.first, list_virtual_disk_chain(_, _, _))
        .WillByDefault(Invoke(
            [&](const fs::path& disk, std::vector<fs::path>& chain, std::optional<std::size_t>) {
                chain = {disk};
                if (disk.filename() == "active.avhdx")
                    chain.push_back(source_dir / "base.vhdx");
                else if (disk.filename() == "snap1.avhdx")
                    chain.push_back(disk.parent_path() / "base.vhdx");
                return mhv::OperationResult::success();
            }));

    EXPECT_THROW(transaction.verify(mapping, layout), std::runtime_error);
}

TEST_F(HyperVTargetTransaction, commitPromotesAndPersistsOwnership)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    const auto mapping = transaction.stage(layout, source_dir);
    transaction.verify(mapping, layout);

    const auto ownership = transaction.commit(mapping, layout, source_dir);

    EXPECT_EQ(ownership.active_disk, target_instance_dir / "active.avhdx");
    EXPECT_TRUE(MP_FILEOPS.exists(target_instance_dir / "active.avhdx"));
    EXPECT_TRUE(MP_FILEOPS.exists(target_instance_dir / "hcs-ownership.json"));
    const auto manifest = mhv::MigrationTransactionManifest::load(target_instance_dir);
    ASSERT_TRUE(manifest);
    EXPECT_EQ(manifest->phase, mhv::MigrationTransactionManifest::prepared_phase_name);
    const auto committed_snapshot = MP_FILEOPS.try_read_file(target_instance_dir /
                                                             "0001.snapshot.json");
    ASSERT_TRUE(committed_snapshot);
    const auto json = boost::json::parse(*committed_snapshot);
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("snapshot").at("disk_path")),
              (target_instance_dir / "snap1.avhdx").string());

    // Source remains intact after a successful commit.
    EXPECT_TRUE(MP_FILEOPS.exists(active));
    EXPECT_FALSE(MP_FILEOPS.exists(source_dir / "hcs-ownership.json"));
}

TEST_F(HyperVTargetTransaction, commitRefusesExistingTargetDirectory)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    const auto mapping = transaction.stage(layout, source_dir);
    fs::create_directories(target_instance_dir);

    EXPECT_THROW((void)transaction.commit(mapping, layout, source_dir), std::runtime_error);
    EXPECT_TRUE(MP_FILEOPS.exists(active));
    EXPECT_FALSE(MP_FILEOPS.exists(target_instance_dir / "active.avhdx"));
}

TEST_F(HyperVTargetTransaction, rollbackRemovesStagingOrphan)
{
    mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
    (void)transaction.stage(layout, source_dir);
    const auto staging = transaction.staging_dir();
    ASSERT_TRUE(MP_FILEOPS.exists(staging));

    transaction.rollback();
    EXPECT_FALSE(MP_FILEOPS.exists(staging));
    // Source is untouched by rollback.
    EXPECT_TRUE(MP_FILEOPS.exists(active));
}

TEST_F(HyperVTargetTransaction, destructorRollsBackUncommittedStaging)
{
    fs::path staging;
    {
        mhv::TargetMigrationTransaction transaction{vm_name, target_instance_dir};
        (void)transaction.stage(layout, source_dir);
        staging = transaction.staging_dir();
        ASSERT_TRUE(MP_FILEOPS.exists(staging));
    }
    EXPECT_FALSE(MP_FILEOPS.exists(staging));
}

TEST(HyperVTargetTransactionSpace, preflightThrowsWhenTargetVolumeIsFull)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject<NiceMock>();
    mhv::LegacyDiskLayout layout;
    layout.all_disks = {"C:/instances/a.vhdx", "C:/instances/b.avhdx"};

    fs::space_info full{};
    full.available = 0;
    EXPECT_CALL(*mock_file_ops, space(_, _)).WillOnce(Return(full));

    mhv::TargetMigrationTransaction transaction{"vm", "C:/instances/vm"};
    EXPECT_THROW(transaction.check_space(layout), std::runtime_error);
}

TEST(HyperVTargetTransactionSpace, preflightPassesWithAmpleSpace)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject<NiceMock>();
    mhv::LegacyDiskLayout layout;
    layout.all_disks = {"C:/instances/a.vhdx"};

    fs::space_info ample{};
    ample.available = std::numeric_limits<std::uintmax_t>::max();
    EXPECT_CALL(*mock_file_ops, file_size(_, _)).WillRepeatedly(Return(1024));
    EXPECT_CALL(*mock_file_ops, space(_, _)).WillOnce(Return(ample));

    mhv::TargetMigrationTransaction transaction{"vm", "C:/instances/vm"};
    EXPECT_NO_THROW(transaction.check_space(layout));
}

TEST(HyperVTrialMacVerification, acceptsMatchingPrimaryAndExtraMacs)
{
    mp::VirtualMachineDescription description;
    description.default_mac_address = "52:54:00:AA:BB:CC";
    description.extra_interfaces = {
        {.id = "eth1", .mac_address = "52:54:00:11:22:33", .auto_mode = true},
        {.id = "eth2", .mac_address = "52:54:00:44:55:66", .auto_mode = true}};

    // Order-independent, case/separator-insensitive.
    EXPECT_NO_THROW(
        mhv::verify_trial_macs(description,
                               {"52-54-00-44-55-66", "52:54:00:aa:bb:cc", "525400112233"}));
}

TEST(HyperVTrialMacVerification, rejectsMissingPrimary)
{
    mp::VirtualMachineDescription description;
    description.default_mac_address = "52:54:00:AA:BB:CC";
    EXPECT_THROW(mhv::verify_trial_macs(description, {"52:54:00:11:22:33"}), std::runtime_error);
}

TEST(HyperVTrialMacVerification, rejectsExtraMacSetMismatch)
{
    mp::VirtualMachineDescription description;
    description.default_mac_address = "52:54:00:AA:BB:CC";
    description.extra_interfaces = {
        {.id = "eth1", .mac_address = "52:54:00:11:22:33", .auto_mode = true}};

    // Wrong extra MAC.
    EXPECT_THROW(mhv::verify_trial_macs(description, {"52:54:00:AA:BB:CC", "52:54:00:99:99:99"}),
                 std::runtime_error);
    // Extra count mismatch (unexpected additional MAC).
    EXPECT_THROW(
        mhv::verify_trial_macs(description,
                               {"52:54:00:AA:BB:CC", "52:54:00:11:22:33", "52:54:00:77:77:77"}),
        std::runtime_error);
}
