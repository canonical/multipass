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
#include "tests/unit/temp_dir.h"

#include <daemon/hyperv_migration.h>

#include <hyperv/hyperv_target_transaction.h>
#include <hyperv_api/hcs_ownership.h>

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>

#include <filesystem>

namespace mp = multipass;
namespace mhv = multipass::hyperv;
namespace mpt = multipass::test;
namespace fs = std::filesystem;
using namespace testing;

namespace
{
mp::VaultRecord image_record(const fs::path& image)
{
    return {.image = {.image_path = image,
                      .id = "image-id",
                      .original_release = "24.04",
                      .current_release = "24.04",
                      .release_date = "20260801",
                      .os = "Ubuntu",
                      .aliases = {"noble"}},
            .query = {.name = "vm",
                      .release = "24.04",
                      .persistent = false,
                      .remote_name = "release",
                      .query_type = mp::Query::Alias},
            .last_accessed = std::chrono::system_clock::now()};
}

mp::VMSpecs vm_spec()
{
    return {.num_cores = 2,
            .mem_size = mp::MemorySize{"2G"},
            .disk_space = mp::MemorySize{"8G"},
            .default_mac_address = "52:54:00:12:34:56",
            .extra_interfaces = {},
            .ssh_username = "ubuntu",
            .state = mp::VirtualMachine::State::stopped,
            .mounts = {},
            .deleted = false,
            .metadata = {{"key", "value"}},
            .clone_count = 3,
            .zone = "zone1"};
}

void write_source_record(const fs::path& data_dir, const std::string& name)
{
    const auto path = data_dir / "vault" / "multipassd-instance-image-records.json";
    const boost::json::object records{
        {name,
         boost::json::value_from(
             image_record(data_dir / "vault" / "instances" / name / "source.vhdx"))}};
    MP_FILEOPS.write_transactionally(path, mp::pretty_print(records));
}

void write_manifest(const fs::path& dir, const std::string& name, const char* phase)
{
    fs::create_directories(dir);
    const mhv::MigrationTransactionManifest manifest{
        .transaction_id = "tx-" + name,
        .phase = phase,
        .vm_name = name,
    };
    manifest.persist(dir);
}

void write_ownership(const fs::path& dir)
{
    const mhv::HCSOwnership ownership{
        .active_disk = dir / "active.avhdx",
        .state_file_stem = dir / "hcs-state",
    };
    ownership.persist(dir);
}

void write_prepared_target(const fs::path& dir, const std::string& name)
{
    write_manifest(dir, name, mhv::MigrationTransactionManifest::prepared_phase_name);
    write_ownership(dir);
}

boost::json::object read_records(const fs::path& path)
{
    const auto contents = MP_FILEOPS.try_read_file(path);
    EXPECT_TRUE(contents);
    return boost::json::parse(*contents).as_object();
}
} // namespace

TEST(HyperVMigrationTargetRecords, commitsImageBeforeVisibleVmAndRemovesManifest)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    write_source_record(data_dir, "vm");

    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.preflight();
    store.prepare();

    const auto target_dir = store.instance_dir("vm");
    write_prepared_target(target_dir, "vm");
    auto record = store.source_image_record("vm");
    store.commit("vm", vm_spec(), record);

    EXPECT_FALSE(MP_FILEOPS.exists(target_dir / mhv::MigrationTransactionManifest::filename));

    const auto target_root = data_dir / "hyperv_api";
    const auto vm_records = read_records(target_root / "multipassd-vm-instances.json");
    const auto image_records = read_records(target_root / "vault" /
                                            "multipassd-instance-image-records.json");
    EXPECT_TRUE(vm_records.contains("vm"));
    ASSERT_TRUE(image_records.contains("vm"));
    EXPECT_EQ(boost::json::value_to<std::string>(image_records.at("vm").at("image").at("path")),
              (target_dir / "active.avhdx").string());
}

TEST(HyperVMigrationTargetRecords, recoveryRemovesOrphanImageRecordAndDirectory)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto target_root = data_dir / "hyperv_api";
    const auto target_dir = target_root / "vault" / "instances" / "orphan";
    write_prepared_target(target_dir, "orphan");

    const boost::json::object image_records{
        {"orphan", boost::json::value_from(image_record(target_dir / "active.avhdx"))}};
    MP_FILEOPS.write_transactionally(target_root / "vault" /
                                         "multipassd-instance-image-records.json",
                                     mp::pretty_print(image_records));

    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.prepare();

    EXPECT_FALSE(MP_FILEOPS.exists(target_dir));
    const auto persisted = read_records(target_root / "vault" /
                                        "multipassd-instance-image-records.json");
    EXPECT_FALSE(persisted.contains("orphan"));
}

TEST(HyperVMigrationTargetRecords, recoveryFinalizesCommittedTarget)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto target_root = data_dir / "hyperv_api";
    const auto target_dir = target_root / "vault" / "instances" / "vm";
    write_prepared_target(target_dir, "vm");

    MP_FILEOPS.write_transactionally(
        target_root / "multipassd-vm-instances.json",
        mp::pretty_print(boost::json::object{{"vm", boost::json::value_from(vm_spec())}}));
    MP_FILEOPS.write_transactionally(
        target_root / "vault" / "multipassd-instance-image-records.json",
        mp::pretty_print(boost::json::object{
            {"vm", boost::json::value_from(image_record(target_dir / "active.avhdx"))}}));

    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.prepare();

    EXPECT_TRUE(MP_FILEOPS.exists(target_dir / "hcs-ownership.json"));
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir / mhv::MigrationTransactionManifest::filename));
}

TEST(HyperVMigrationTargetRecords, recoveryLeavesIncompleteCommittedTarget)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto target_root = data_dir / "hyperv_api";
    const auto target_dir = target_root / "vault" / "instances" / "vm";
    write_manifest(target_dir, "vm", mhv::MigrationTransactionManifest::prepared_phase_name);

    MP_FILEOPS.write_transactionally(
        target_root / "multipassd-vm-instances.json",
        mp::pretty_print(boost::json::object{{"vm", boost::json::value_from(vm_spec())}}));
    MP_FILEOPS.write_transactionally(
        target_root / "vault" / "multipassd-instance-image-records.json",
        mp::pretty_print(boost::json::object{
            {"vm", boost::json::value_from(image_record(target_dir / "active.avhdx"))}}));

    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.prepare();

    EXPECT_TRUE(MP_FILEOPS.exists(target_dir / mhv::MigrationTransactionManifest::filename));
}

TEST(HyperVMigrationTargetRecords, recoveryOnlyRemovesOwnedStagingDirectories)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto instances_root = data_dir / "hyperv_api" / "vault" / "instances";
    const auto owned = instances_root / (std::string{mhv::migration_staging_prefix} + "vm-tx");
    const auto unowned = instances_root /
                         (std::string{mhv::migration_staging_prefix} + "user-data");
    write_manifest(owned, "vm", mhv::MigrationTransactionManifest::staged_phase_name);
    fs::create_directories(unowned);
    MP_FILEOPS.write_transactionally(unowned / "keep", "keep");

    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.prepare();

    EXPECT_FALSE(MP_FILEOPS.exists(owned));
    EXPECT_TRUE(MP_FILEOPS.exists(unowned / "keep"));
}

TEST(HyperVMigrationTargetRecords, recoveryIgnoresMalformedManifest)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto instances_root = data_dir / "hyperv_api" / "vault" / "instances";
    const auto malformed = instances_root / "malformed";
    fs::create_directories(malformed);
    MP_FILEOPS.write_transactionally(malformed / mhv::MigrationTransactionManifest::filename,
                                     R"({"version":999})");
    write_prepared_target(instances_root / "orphan", "orphan");

    mhv::HyperVMigrationTargetRecords store{data.path()};
    EXPECT_NO_THROW(store.prepare());

    EXPECT_TRUE(MP_FILEOPS.exists(malformed / mhv::MigrationTransactionManifest::filename));
    EXPECT_FALSE(MP_FILEOPS.exists(instances_root / "orphan"));
}

TEST(HyperVMigrationTargetRecords, recoveryRemovesRecordOnlyOrphan)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto target_root = data_dir / "hyperv_api";
    MP_FILEOPS.write_transactionally(
        target_root / "vault" / "multipassd-instance-image-records.json",
        mp::pretty_print(boost::json::object{
            {"orphan", boost::json::value_from(image_record(target_root / "missing.vhdx"))}}));

    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.prepare();

    EXPECT_FALSE(store.target_exists("orphan"));
    EXPECT_FALSE(read_records(target_root / "vault" / "multipassd-instance-image-records.json")
                     .contains("orphan"));
}

TEST(HyperVMigrationTargetRecords, deletedVmRecordStillCollides)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto target_root = data_dir / "hyperv_api";
    auto deleted = vm_spec();
    deleted.deleted = true;
    const boost::json::object vm_records{{"vm", boost::json::value_from(deleted)}};
    MP_FILEOPS.write_transactionally(target_root / "multipassd-vm-instances.json",
                                     mp::pretty_print(vm_records));

    mhv::HyperVMigrationTargetRecords store{data.path()};

    EXPECT_TRUE(store.target_exists("vm"));
}
