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
#include "tests/unit/temp_dir.h"

#include <src/platform/backends/hyperv/hyperv_target_store.h>
#include <src/platform/backends/hyperv/hyperv_target_transaction.h>

#include <hyperv_api/hcs_ownership.h>
#include <multipass/file_ops.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
namespace fs = std::filesystem;
using namespace testing;

namespace
{
void write_manifest(const fs::path& dir, const std::string& vm, const char* phase)
{
    fs::create_directories(dir);
    mhv::MigrationTransactionManifest manifest;
    manifest.transaction_id = "tx-" + vm;
    manifest.vm_name = vm;
    manifest.phase = phase;
    manifest.active_disk = dir / "active.avhdx";
    manifest.state_file_stem = dir / "hcs-migrated-state";
    manifest.owned_disks = {dir / "active.avhdx"};
    manifest.persist(dir);
}

void write_ownership(const fs::path& dir)
{
    fs::create_directories(dir);
    const mhv::HCSOwnership ownership{.active_disk = dir / "active.avhdx",
                                      .state_file_stem = dir / "hcs-migrated-state"};
    ownership.persist(dir);
}

auto no_records()
{
    return [](const std::string&) { return false; };
}

auto all_records()
{
    return [](const std::string&) { return true; };
}

bool contains(const std::vector<std::string>& v, const std::string& s)
{
    return std::find(v.begin(), v.end(), s) != v.end();
}

struct HyperVTargetStoreTest : public Test
{
    HyperVTargetStoreTest()
    {
        instances_root = fs::path{data.path().toStdString()} / "hyperv_api" / "instances";
        fs::create_directories(instances_root);
    }

    mpt::TempDir data;
    fs::path instances_root;
};
} // namespace

TEST_F(HyperVTargetStoreTest, recoverRemovesPreCommitOrphan)
{
    const auto orphan = instances_root / "orphan";
    write_manifest(orphan, "orphan", mhv::MigrationTransactionManifest::prepared_phase_name);

    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, no_records(), no_records());

    EXPECT_FALSE(MP_FILEOPS.exists(orphan));
    EXPECT_TRUE(contains(report.removed_precommit, "orphan"));
    EXPECT_TRUE(report.finalized.empty());
}

TEST_F(HyperVTargetStoreTest, recoverRemovesPromotedStagedOrphan)
{
    const auto orphan = instances_root / "orphan";
    write_manifest(orphan, "orphan", mhv::MigrationTransactionManifest::staged_phase_name);

    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, no_records(), no_records());

    EXPECT_FALSE(MP_FILEOPS.exists(orphan));
    EXPECT_TRUE(contains(report.removed_precommit, "orphan"));
}

TEST_F(HyperVTargetStoreTest, recoverFinalizesCommittedTargetWithStaleManifest)
{
    const auto live = instances_root / "live";
    write_manifest(live, "live", mhv::MigrationTransactionManifest::prepared_phase_name);
    write_ownership(live);

    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, all_records(), all_records());

    // The visible target is preserved; only the stale manifest is dropped.
    EXPECT_TRUE(MP_FILEOPS.exists(live));
    EXPECT_TRUE(MP_FILEOPS.exists(live / "hcs-ownership.json"));
    EXPECT_FALSE(MP_FILEOPS.exists(live / mhv::MigrationTransactionManifest::filename));
    EXPECT_TRUE(contains(report.finalized, "live"));
    EXPECT_TRUE(report.removed_precommit.empty());
}

TEST_F(HyperVTargetStoreTest, recoverRemovesStagingSiblings)
{
    const auto staging = instances_root /
                         (std::string{mhv::HyperVTargetStore::staging_prefix} + "live-abcd1234");
    write_manifest(staging, "live", mhv::MigrationTransactionManifest::staged_phase_name);

    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, no_records(), no_records());

    EXPECT_FALSE(MP_FILEOPS.exists(staging));
    // Staging siblings are not named after the instance, so they need no image-record cleanup.
    EXPECT_TRUE(report.removed_precommit.empty());
}

TEST_F(HyperVTargetStoreTest, recoverLeavesUnownedStagingNameUntouched)
{
    const auto staging = instances_root /
                         (std::string{mhv::HyperVTargetStore::staging_prefix} + "user-data");
    fs::create_directories(staging);
    mpt::make_file_with_content(QString::fromStdString((staging / "keep").string()), "keep");

    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, no_records(), no_records());

    EXPECT_TRUE(MP_FILEOPS.exists(staging / "keep"));
    EXPECT_TRUE(report.removed_precommit.empty());
    EXPECT_TRUE(report.finalized.empty());
}

TEST_F(HyperVTargetStoreTest, recoverLeavesManifestlessDirectoriesUntouched)
{
    const auto normal = instances_root / "normal";
    fs::create_directories(normal);
    mpt::make_file_with_content(QString::fromStdString((normal / "0000.json").string()), "{}");

    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, no_records(), no_records());

    EXPECT_TRUE(MP_FILEOPS.exists(normal / "0000.json"));
    EXPECT_TRUE(report.removed_precommit.empty());
    EXPECT_TRUE(report.finalized.empty());
}

TEST_F(HyperVTargetStoreTest, recoverKeepsManifestWhenCommittedTargetLacksOwnership)
{
    const auto inconsistent = instances_root / "inconsistent";
    write_manifest(inconsistent,
                   "inconsistent",
                   mhv::MigrationTransactionManifest::prepared_phase_name);

    // VM record exists but ownership marker is missing: refuse to touch, leave for inspection.
    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, all_records(), no_records());

    EXPECT_TRUE(MP_FILEOPS.exists(inconsistent / mhv::MigrationTransactionManifest::filename));
    EXPECT_TRUE(report.finalized.empty());
    EXPECT_TRUE(report.removed_precommit.empty());
}

TEST_F(HyperVTargetStoreTest, recoverHandlesMixedRoot)
{
    write_manifest(instances_root / "orphan",
                   "orphan",
                   mhv::MigrationTransactionManifest::prepared_phase_name);
    write_manifest(instances_root / "committed",
                   "committed",
                   mhv::MigrationTransactionManifest::prepared_phase_name);
    write_ownership(instances_root / "committed");
    write_manifest(instances_root /
                       (std::string{mhv::HyperVTargetStore::staging_prefix} + "x-1"),
                   "x",
                   mhv::MigrationTransactionManifest::staged_phase_name);
    fs::create_directories(instances_root / "plain");

    const auto committed = [](const std::string& name) { return name == "committed"; };
    const auto report =
        mhv::HyperVTargetStore::recover(instances_root, committed, committed);

    EXPECT_FALSE(MP_FILEOPS.exists(instances_root / "orphan"));
    EXPECT_TRUE(MP_FILEOPS.exists(instances_root / "committed"));
    EXPECT_FALSE(MP_FILEOPS.exists(
        instances_root / "committed" / mhv::MigrationTransactionManifest::filename));
    EXPECT_TRUE(MP_FILEOPS.exists(instances_root / "plain"));
    EXPECT_TRUE(contains(report.removed_precommit, "orphan"));
    EXPECT_TRUE(contains(report.finalized, "committed"));
}

TEST_F(HyperVTargetStoreTest, recoverIgnoresMalformedManifestWithoutAborting)
{
    // An unparseable/invalid manifest must be treated as "no valid manifest": the enclosing
    // directory is left untouched and the sweep keeps going instead of aborting.
    const auto bad = instances_root / "bad";
    fs::create_directories(bad);
    mpt::make_file_with_content(
        QString::fromStdString((bad / mhv::MigrationTransactionManifest::filename).string()),
        R"({"version": 999})");

    // A valid orphan alongside it must still be processed even though "bad" came first.
    write_manifest(instances_root / "orphan",
                   "orphan",
                   mhv::MigrationTransactionManifest::prepared_phase_name);

    mhv::TargetRecoveryReport report;
    EXPECT_NO_THROW(
        report = mhv::HyperVTargetStore::recover(instances_root, no_records(), no_records()));

    // The malformed directory survives untouched...
    EXPECT_TRUE(MP_FILEOPS.exists(bad / mhv::MigrationTransactionManifest::filename));
    EXPECT_FALSE(contains(report.removed_precommit, "bad"));
    EXPECT_FALSE(contains(report.finalized, "bad"));
    // ...and the valid orphan was still cleaned up.
    EXPECT_FALSE(MP_FILEOPS.exists(instances_root / "orphan"));
    EXPECT_TRUE(contains(report.removed_precommit, "orphan"));
}

TEST_F(HyperVTargetStoreTest, targetExistsWhenFinalDirectoryPresent)
{
    fs::create_directories(instances_root / "vm");
    EXPECT_TRUE(mhv::HyperVTargetStore::target_exists(instances_root, "vm", no_records()));
}

TEST_F(HyperVTargetStoreTest, targetExistsWhenOwnershipMarkerPresent)
{
    write_ownership(instances_root / "vm");
    EXPECT_TRUE(mhv::HyperVTargetStore::target_exists(instances_root, "vm", no_records()));
}

TEST_F(HyperVTargetStoreTest, targetExistsWhenRecordPresent)
{
    EXPECT_TRUE(mhv::HyperVTargetStore::target_exists(instances_root, "vm", all_records()));
}

TEST_F(HyperVTargetStoreTest, targetDoesNotExistForFreshName)
{
    EXPECT_FALSE(mhv::HyperVTargetStore::target_exists(instances_root, "vm", no_records()));
}
