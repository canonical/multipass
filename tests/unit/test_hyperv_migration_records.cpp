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
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/stub_availability_zone_manager.h"
#include "tests/unit/stub_virtual_machine_factory.h"
#include "tests/unit/temp_dir.h"
#include "tests/unit/windows/powershell_test_helper.h"

#include <daemon/hyperv_migration.h>

#include <hyperv/hyperv_target_transaction.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>

#include <filesystem>
#include <future>
#include <mutex>

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

void write_prepared_target(const fs::path& dir, const std::string& name)
{
    write_manifest(dir, name, mhv::MigrationTransactionManifest::prepared_phase_name);
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
    mhv::HyperVMigrationTargetRecords store{data.path()};
    store.prepare();

    EXPECT_TRUE(MP_FILEOPS.exists(target_dir / mhv::MigrationTransactionManifest::filename));
}

TEST(HyperVMigrationTargetRecords, recoveryOnlyRemovesManifestOwnedDirectories)
{
    mpt::TempDir data;
    const auto data_dir = fs::path{data.path().toStdString()};
    const auto instances_root = data_dir / "hyperv_api" / "vault" / "instances";
    const auto owned = instances_root / "vm";
    const auto unowned = instances_root / "user-data";
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

namespace
{
struct MockMigrationProgress : mhv::MigrationProgress
{
    MOCK_METHOD(void, phase, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, skipped, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, failed, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, finished, (const std::vector<std::string>&), (override));
};

struct HyperVMigrationEligibility : Test
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mp::logging::Level::error);
        const auto data_dir = fs::path{data.path().toStdString()};
        write_source_record(data_dir, "vm");
        MP_FILEOPS.write_transactionally(
            data_dir / "hyperv_api" / "multipassd-vm-instances.json",
            mp::pretty_print(boost::json::object{{"vm", boost::json::value_from(vm_spec())}}));

        vm->state = mp::VirtualMachine::State::stopped;
        EXPECT_CALL(*vm, current_state()).Times(0);
    }

    mhv::InstanceMigrationResult migrate()
    {
        mhv::HyperVMigrationTargetRecords store{data.path()};
        mhv::DaemonHyperVInstanceMigrator
            migrator{specs, instances, deleted_instances, factory, az_manager, data.path(), store};
        return migrator.migrate("vm", progress);
    }

    void mock_query(QByteArray output,
                    QByteArray error = {},
                    int exit_code = 0,
                    bool finished = true)
    {
        ps_helper.setup(
            [this, output, error, exit_code, finished](auto* process) {
                EXPECT_EQ(process->program(), "powershell.exe");
                EXPECT_THAT(process->arguments(),
                            ElementsAre("-NoProfile",
                                        "-NonInteractive",
                                        "-Command",
                                        "Get-VM -Name 'vm' -ErrorAction Stop | "
                                        "Select-Object -ExpandProperty State"));
                EXPECT_CALL(*process, write(_)).Times(0);
                EXPECT_CALL(*process, start()).WillOnce([process] {
                    emit process->ready_read_standard_output();
                    emit process->ready_read_standard_error();
                });
                EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(output));
                EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return(error));
                EXPECT_CALL(*process, wait_for_finished(60000)).WillOnce([this, finished](int) {
                    auto lock_available = std::async(std::launch::async, [this] {
                        const std::unique_lock lock{vm->state_mutex, std::try_to_lock};
                        return lock.owns_lock();
                    });
                    EXPECT_TRUE(lock_available.get());
                    return finished;
                });
                ON_CALL(*process, process_state())
                    .WillByDefault(Return(mp::ProcessState{exit_code, std::nullopt}));
            },
            /* auto_exit = */ false);
    }

    mpt::TempDir data;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mpt::PowerShellTestHelper ps_helper;
    mpt::StubAvailabilityZoneManager az_manager;
    mpt::StubVirtualMachineFactory factory{az_manager};
    std::shared_ptr<StrictMock<mpt::MockVirtualMachine>> vm =
        std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    std::unordered_map<std::string, mp::VMSpecs> specs{{"vm", vm_spec()}};
    mhv::DaemonHyperVInstanceMigrator::InstanceTable instances{{"vm", vm}};
    mhv::DaemonHyperVInstanceMigrator::InstanceTable deleted_instances;
    StrictMock<MockMigrationProgress> progress;
};
} // namespace

TEST_F(HyperVMigrationEligibility, skipsStartingWithoutQueryingPowerShell)
{
    vm->state = mp::VirtualMachine::State::starting;

    EXPECT_EQ(migrate(), "instance is starting and needs to be stopped");
    EXPECT_FALSE(ps_helper.was_ps_run());
}

TEST_F(HyperVMigrationEligibility, skipsRestartingWithoutQueryingPowerShell)
{
    vm->state = mp::VirtualMachine::State::restarting;

    EXPECT_EQ(migrate(), "instance is restarting and needs to be stopped");
    EXPECT_FALSE(ps_helper.was_ps_run());
}

TEST_F(HyperVMigrationEligibility, confirmsStoppedStateBeforeCheckingTargetCollision)
{
    mock_query("Off\r\n");

    EXPECT_EQ(migrate(), "name already taken by a hyperv_api instance");
    EXPECT_TRUE(ps_helper.was_ps_run());
}

TEST_F(HyperVMigrationEligibility, acceptsHostStoppedStateDespiteStaleRunningState)
{
    vm->state = mp::VirtualMachine::State::running;
    mock_query("Off");

    EXPECT_EQ(migrate(), "name already taken by a hyperv_api instance");
}

TEST_F(HyperVMigrationEligibility, skipsNonStoppedHostDespiteCachedStoppedState)
{
    for (const auto* state : {"Running", "Saved", "Starting", "Stopping", "Paused", "Unknown"})
    {
        SCOPED_TRACE(state);
        mock_query(state);

        EXPECT_THAT(migrate(),
                    Optional("Hyper-V reports state '" + std::string{state} +
                             "' and the instance needs to be stopped"));
    }
}

TEST_F(HyperVMigrationEligibility, reportsFailedStateQuery)
{
    mock_query("", "VM not found", 1);

    MP_EXPECT_THROW_THAT(
        migrate(),
        mhv::InstanceMigrationError,
        Property(&std::exception::what,
                 HasSubstr("Could not query Hyper-V state for 'vm': VM not found")));
}

TEST_F(HyperVMigrationEligibility, reportsStateQueryTimeoutEvenWithOffOutput)
{
    mock_query("Off", {}, 0, /* finished = */ false);

    MP_EXPECT_THROW_THAT(
        migrate(),
        mhv::InstanceMigrationError,
        Property(&std::exception::what, HasSubstr("Could not query Hyper-V state for 'vm'")));
}

TEST_F(HyperVMigrationEligibility, reportsEmptyStateQuery)
{
    mock_query(" \r\n");

    MP_EXPECT_THROW_THAT(
        migrate(),
        mhv::InstanceMigrationError,
        Property(&std::exception::what, HasSubstr("Hyper-V returned no state for 'vm'")));
}
