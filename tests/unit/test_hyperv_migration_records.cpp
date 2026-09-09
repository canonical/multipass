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
#include "tests/unit/hyperv_api/mock_hyperv_virtdisk_wrapper.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/stub_availability_zone_manager.h"
#include "tests/unit/stub_virtual_machine_factory.h"
#include "tests/unit/temp_dir.h"
#include "tests/unit/windows/powershell_test_helper.h"

#include <daemon/hyperv_migration.h>

#include <hyperv_migration/hyperv_target_transaction.h>
#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>

#include <filesystem>
#include <future>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
        return migrator.migrate("vm", report.AsStdFunction());
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
    StrictMock<MockFunction<void(mhv::MigrationMessage, const std::string&)>> report;
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

namespace
{
using MigrationMessages = std::vector<std::pair<mhv::MigrationMessage, std::string>>;

std::string migration_summary(std::initializer_list<std::string> names)
{
    if (names.size() == 0)
        return "No instances were migrated.\n";

    std::string summary{"The following instances were successfully migrated:"};
    for (const auto& name : names)
        summary += "\n  " + name;

    return summary +
           "\n\nThe original hyperv instances were retained. Do not run an original and its "
           "hyperv_api copy at the same time because they share guest identity and MAC "
           "addresses.\n\n"
           "After validating the migrated instances, remove the originals with:\n"
           "  multipass set local.driver=hyperv\n"
           "  multipass delete --purge <instance-name>\n"
           "  multipass set local.driver=hyperv_api\n";
}

MigrationMessages successful_messages(std::initializer_list<std::string> names)
{
    MigrationMessages messages;
    for (const auto& name : names)
        for (const auto* phase : {"Preparing networking",
                                  "inspecting source disk layout",
                                  "copying disks to the target instance",
                                  "verifying the copied disks",
                                  "committing the migrated instance",
                                  "Committing target records"})
            messages.emplace_back(mhv::MigrationMessage::phase, std::string{phase} + ": " + name);
    messages.emplace_back(mhv::MigrationMessage::summary, migration_summary(names));
    return messages;
}

struct HyperVBulkMigration : Test
{
    void SetUp() override
    {
        ps_helper.setup(
            [this](auto* process) {
                EXPECT_EQ(process->program(), "powershell.exe");
                ON_CALL(*process, write(_))
                    .WillByDefault(Return(mpt::PowerShellTestHelper::written));
                ON_CALL(*process, read_all_standard_output())
                    .WillByDefault(Return(ps_helper.end_marker(false)));
                ON_CALL(*process, wait_for_finished(_)).WillByDefault(Return(true));
                ASSERT_LT(next_process, process_setups.size());
                process_setups[next_process++](process);
            },
            /* auto_exit = */ false);

        ON_CALL(*virtdisk.first, list_virtual_disk_chain(_, _, _))
            .WillByDefault(
                [](const fs::path& disk, std::vector<fs::path>& chain, std::optional<std::size_t>) {
                    chain = {disk};
                    if (disk.filename() == "active.avhdx")
                        chain.push_back(disk.parent_path() / "base.vhdx");
                    return mhv::OperationResult::success();
                });
        ON_CALL(*virtdisk.first, reparent_virtual_disk(_, _))
            .WillByDefault([this](const fs::path& child, const fs::path& parent) {
                EXPECT_EQ(child.parent_path(), parent.parent_path());
                EXPECT_EQ(child.parent_path().parent_path(), target_root / "vault" / "instances");
                return mhv::OperationResult::success();
            });
    }

    void TearDown() override
    {
        EXPECT_EQ(next_process, process_setups.size());
    }

    fs::path source_dir(const std::string& name) const
    {
        return data_dir / "vault" / "instances" / name;
    }

    fs::path target_dir(const std::string& name) const
    {
        return target_root / "vault" / "instances" / name;
    }

    void add_instance(const std::string& name)
    {
        auto vm = std::make_shared<NiceMock<mpt::MockVirtualMachine>>();
        vm->state = mp::VirtualMachine::State::stopped;
        ON_CALL(*vm, instance_directory())
            .WillByDefault(Return(QDir{QString::fromStdWString(source_dir(name).wstring())}));
        EXPECT_CALL(*vm, current_state()).Times(0);
        EXPECT_CALL(*vm, start()).Times(0);
        EXPECT_CALL(*vm, shutdown(_)).Times(0);
        EXPECT_CALL(*vm, suspend()).Times(0);
        instances.emplace(name, std::move(vm));
        specs.emplace(name, vm_spec());
        source_images[name] = boost::json::value_from(
            image_record(source_dir(name) / "active.avhdx"));
        MP_FILEOPS.write_transactionally(source_dir(name) / "active.avhdx", "active-" + name);
        MP_FILEOPS.write_transactionally(source_dir(name) / "base.vhdx", "base-" + name);
        MP_FILEOPS.write_transactionally(source_dir(name) / mp::cloud_init_file_name,
                                         "cloud-init-" + name);
    }

    void expect_state_query(const std::string& name,
                            QByteArray output = "Off\r\n",
                            QByteArray error = {},
                            int exit_code = 0)
    {
        process_setups.push_back([name, output, error, exit_code](auto* process) {
            EXPECT_THAT(process->arguments(),
                        ElementsAre("-NoProfile",
                                    "-NonInteractive",
                                    "-Command",
                                    QString::fromStdString("Get-VM -Name '" + name +
                                                           "' -ErrorAction Stop | "
                                                           "Select-Object -ExpandProperty State")));
            EXPECT_CALL(*process, write(_)).Times(0);
            EXPECT_CALL(*process, start()).WillOnce([process] {
                emit process->ready_read_standard_output();
                emit process->ready_read_standard_error();
            });
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(output));
            EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return(error));
            EXPECT_CALL(*process, wait_for_finished(60000)).WillOnce(Return(true));
            ON_CALL(*process, process_state())
                .WillByDefault(Return(mp::ProcessState{exit_code, std::nullopt}));
        });
    }

    void expect_migration(const std::string& name)
    {
        expect_state_query(name);
        process_setups.push_back([this, name](auto* process) {
            EXPECT_THAT(process->arguments(),
                        ElementsAre("-NoProfile", "-NoExit", "-Command", "-"));
            EXPECT_CALL(*process, start()).Times(1);
            InSequence sequence;
            ps_helper.expect_writes(process,
                                    QByteArray::fromStdString("$vmName='" + name +
                                                              "'; $primary=@(Get-VMHardDiskDrive"));
            const boost::json::object discovery{
                {"ActiveDisk", (source_dir(name) / "active.avhdx").string()},
                {"Snapshots", boost::json::array{}}};
            EXPECT_CALL(*process, read_all_standard_output())
                .WillOnce(Return(QByteArray::fromStdString(boost::json::serialize(discovery))
                                     .append(ps_helper.end_marker(true))));
            EXPECT_CALL(*process, write(Eq(mpt::PowerShellTestHelper::psexit)))
                .WillOnce(Return(mpt::PowerShellTestHelper::written));
            EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));
        });
    }

    mhv::MigrationOutcome run(const mhv::MigrationCancellation& cancel = [] { return false; })
    {
        MP_FILEOPS.write_transactionally(data_dir / "vault" /
                                             "multipassd-instance-image-records.json",
                                         mp::pretty_print(source_images));
        mhv::HyperVMigrationTargetRecords store{data.path()};
        store.preflight();
        store.prepare();
        mhv::DaemonHyperVInstanceMigrator
            migrator{specs, instances, deleted_instances, factory, az_manager, data.path(), store};
        return mhv::run_bulk_migration(migrator, report, cancel);
    }

    void expect_source_retained(const std::string& name)
    {
        EXPECT_THAT(MP_FILEOPS.try_read_file(source_dir(name) / "active.avhdx"),
                    Optional("active-" + name));
        EXPECT_THAT(MP_FILEOPS.try_read_file(source_dir(name) / "base.vhdx"),
                    Optional("base-" + name));
        EXPECT_THAT(MP_FILEOPS.try_read_file(source_dir(name) / mp::cloud_init_file_name),
                    Optional("cloud-init-" + name));
        EXPECT_FALSE(
            MP_FILEOPS.exists(source_dir(name) / mhv::MigrationTransactionManifest::filename));
        EXPECT_EQ(read_records(data_dir / "vault" / "multipassd-instance-image-records.json"),
                  source_images);
        EXPECT_EQ(boost::json::value_from(specs.at(name)), boost::json::value_from(vm_spec()));
        EXPECT_TRUE(instances.contains(name));
    }

    void expect_committed(const std::string& name)
    {
        const auto vm_records = read_records(target_root / "multipassd-vm-instances.json");
        const auto image_records = read_records(target_root / "vault" /
                                                "multipassd-instance-image-records.json");
        ASSERT_TRUE(vm_records.contains(name));
        ASSERT_TRUE(image_records.contains(name));
        EXPECT_EQ(vm_records.at(name), boost::json::value_from(specs.at(name)));
        auto expected_image = source_images.at(name);
        expected_image.at("image").at("path") = (target_dir(name) / "active.avhdx").string();
        EXPECT_EQ(image_records.at(name), expected_image);
        EXPECT_THAT(MP_FILEOPS.try_read_file(target_dir(name) / "active.avhdx"),
                    Optional("active-" + name));
        EXPECT_THAT(MP_FILEOPS.try_read_file(target_dir(name) / "base.vhdx"),
                    Optional("base-" + name));
        EXPECT_THAT(MP_FILEOPS.try_read_file(target_dir(name) / mp::cloud_init_file_name),
                    Optional("cloud-init-" + name));
        EXPECT_FALSE(
            MP_FILEOPS.exists(target_dir(name) / mhv::MigrationTransactionManifest::filename));
        expect_source_retained(name);
    }

    mpt::TempDir data;
    const fs::path data_dir{data.path().toStdWString()};
    const fs::path target_root{data_dir / "hyperv_api"};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mpt::PowerShellTestHelper ps_helper;
    decltype(mpt::MockVirtDiskWrapper::inject<NiceMock>()) virtdisk{
        mpt::MockVirtDiskWrapper::inject<NiceMock>()};
    mpt::StubAvailabilityZoneManager az_manager;
    mpt::StubVirtualMachineFactory factory{az_manager};
    std::unordered_map<std::string, mp::VMSpecs> specs;
    mhv::DaemonHyperVInstanceMigrator::InstanceTable instances;
    mhv::DaemonHyperVInstanceMigrator::InstanceTable deleted_instances;
    boost::json::object source_images;
    std::vector<mpt::MockProcessFactory::Callback> process_setups;
    std::size_t next_process{0};
    MigrationMessages messages;
    mhv::MigrationReporter report = [this](mhv::MigrationMessage kind, const std::string& message) {
        messages.emplace_back(kind, message);
    };
};
} // namespace

TEST_F(HyperVBulkMigration, processesNamesLexicographicallyAndRetainsSourceCopies)
{
    for (const auto* name : {"zeta", "alpha", "mike", "bravo"})
        add_instance(name);
    for (const auto* name : {"alpha", "bravo", "mike", "zeta"})
        expect_migration(name);

    EXPECT_EQ(run(), mhv::MigrationOutcome::completed);

    EXPECT_EQ(messages, successful_messages({"alpha", "bravo", "mike", "zeta"}));
    for (const auto* name : {"alpha", "bravo", "mike", "zeta"})
        expect_committed(name);
}

TEST_F(HyperVBulkMigration, emptyBatchReportsEmptySummary)
{
    StrictMock<MockFunction<bool()>> cancel;
    EXPECT_CALL(cancel, Call()).Times(0);

    EXPECT_EQ(run(cancel.AsStdFunction()), mhv::MigrationOutcome::completed);

    EXPECT_EQ(messages, successful_messages({}));
    EXPECT_FALSE(ps_helper.was_ps_run());
}

TEST_F(HyperVBulkMigration, skipsAloneAreSuccess)
{
    add_instance("b");
    add_instance("a");
    specs.at("b").deleted = true;
    expect_state_query("a", "Running");

    EXPECT_EQ(run(), mhv::MigrationOutcome::completed);

    EXPECT_THAT(messages,
                ElementsAre(Pair(mhv::MigrationMessage::diagnostic,
                                 "Cannot migrate a: Hyper-V reports state 'Running' and the "
                                 "instance needs to be stopped\n"),
                            Pair(mhv::MigrationMessage::diagnostic,
                                 "Cannot migrate b: instance is deleted\n"),
                            Pair(mhv::MigrationMessage::summary, migration_summary({}))));
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("a")));
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("b")));
    EXPECT_FALSE(MP_FILEOPS.exists(target_root / "multipassd-vm-instances.json"));
}

TEST_F(HyperVBulkMigration, recoverableFailureIsReportedAndProcessingContinues)
{
    for (const auto* name : {"a", "b", "c"})
        add_instance(name);
    expect_migration("a");
    expect_state_query("b", "", "state query failed", 1);
    expect_migration("c");

    EXPECT_EQ(run(), mhv::MigrationOutcome::completed_with_failures);

    auto expected = successful_messages({"a", "c"});
    expected.insert(expected.begin() + 6,
                    {mhv::MigrationMessage::diagnostic,
                     "Failed to migrate b: Could not query Hyper-V state for 'b': "
                     "state query failed\n"});
    EXPECT_EQ(messages, expected);
    expect_committed("a");
    expect_committed("c");
    expect_source_retained("b");
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("b")));
    EXPECT_FALSE(read_records(target_root / "multipassd-vm-instances.json").contains("b"));
}

TEST_F(HyperVBulkMigration, unsafeTargetStoreFailureAbortsRemaining)
{
    for (const auto* name : {"a", "b", "c"})
        add_instance(name);
    expect_migration("a");
    expect_migration("b");

    std::optional<mpt::MockFileOps::GuardedMock> failing_file_ops;
    const auto record_messages = report;
    report = [&, record_messages](mhv::MigrationMessage kind, const std::string& message) {
        record_messages(kind, message);
        if (kind != mhv::MigrationMessage::phase || message != "Committing target records: b")
            return;

        const auto manifest_path = target_dir("b") / mhv::MigrationTransactionManifest::filename;
        const auto manifest = MP_FILEOPS.try_read_file(manifest_path);
        ASSERT_TRUE(manifest);
        // Keep the real prepared disks and fail the record store's write, not the reporter.
        failing_file_ops.emplace(mpt::MockFileOps::inject<StrictMock>());
        EXPECT_CALL(*failing_file_ops->first, try_read_file(manifest_path))
            .WillOnce(Return(manifest));
        EXPECT_CALL(
            *failing_file_ops->first,
            write_transactionally(
                QDir::fromNativeSeparators(QString::fromStdWString(
                    (target_root / "vault" / "multipassd-instance-image-records.json").wstring())),
                _))
            .WillOnce(Throw(std::runtime_error{"target store write failed"}));
    };

    EXPECT_EQ(run(), mhv::MigrationOutcome::aborted);
    ASSERT_TRUE(failing_file_ops);
    failing_file_ops.reset();

    auto expected = successful_messages({"a", "b"});
    expected.back().second = migration_summary({"a"});
    expected.insert(expected.end() - 1,
                    {mhv::MigrationMessage::diagnostic,
                     "Failed to migrate b: could not persist target image record for 'b': "
                     "target store write failed\n"});
    EXPECT_EQ(messages, expected);
    expect_committed("a");
    expect_source_retained("b");
    expect_source_retained("c");
    EXPECT_FALSE(read_records(target_root / "multipassd-vm-instances.json").contains("b"));
    EXPECT_FALSE(read_records(target_root / "vault" / "multipassd-instance-image-records.json")
                     .contains("b"));
    const auto manifest = mhv::MigrationTransactionManifest::load(target_dir("b"));
    ASSERT_TRUE(manifest);
    EXPECT_EQ(manifest->phase, mhv::MigrationTransactionManifest::prepared_phase_name);
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("c")));
}

TEST_F(HyperVBulkMigration, cancellationBeforeAnyInstanceMigratesNothing)
{
    add_instance("a");
    add_instance("b");
    StrictMock<MockFunction<bool()>> cancel;
    EXPECT_CALL(cancel, Call()).WillOnce(Return(true));

    EXPECT_EQ(run(cancel.AsStdFunction()), mhv::MigrationOutcome::cancelled);

    EXPECT_EQ(messages, successful_messages({}));
    EXPECT_FALSE(ps_helper.was_ps_run());
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("a")));
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("b")));
    EXPECT_FALSE(MP_FILEOPS.exists(target_root / "multipassd-vm-instances.json"));
    expect_source_retained("a");
    expect_source_retained("b");
}

TEST_F(HyperVBulkMigration, cancellationAfterCommitRetainsEarlierCopies)
{
    add_instance("a");
    add_instance("b");
    expect_migration("a");
    int cancellation_checks = 0;

    EXPECT_EQ(run([&] {
                  ++cancellation_checks;
                  const auto vm_db = target_root / "multipassd-vm-instances.json";
                  return MP_FILEOPS.exists(vm_db) && read_records(vm_db).contains("a");
              }),
              mhv::MigrationOutcome::cancelled);

    EXPECT_EQ(cancellation_checks, 2);
    EXPECT_EQ(messages, successful_messages({"a"}));
    expect_committed("a");
    expect_source_retained("b");
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("b")));
}

TEST_F(HyperVBulkMigration, cancellationDuringMigrationFinishesCurrentTransaction)
{
    add_instance("a");
    add_instance("b");
    expect_migration("a");
    bool cancelled = false;
    const auto record_messages = report;
    report = [&, record_messages](mhv::MigrationMessage kind, const std::string& message) {
        record_messages(kind, message);
        if (kind == mhv::MigrationMessage::phase &&
            message == "copying disks to the target instance: a")
        {
            EXPECT_FALSE(MP_FILEOPS.exists(target_dir("a")));
            cancelled = true;
        }
    };
    StrictMock<MockFunction<bool()>> cancel;
    {
        InSequence sequence;
        EXPECT_CALL(cancel, Call()).WillOnce([&] {
            EXPECT_FALSE(cancelled);
            return cancelled;
        });
        EXPECT_CALL(cancel, Call()).WillOnce([&] {
            EXPECT_TRUE(cancelled);
            expect_committed("a");
            return cancelled;
        });
    }

    EXPECT_EQ(run(cancel.AsStdFunction()), mhv::MigrationOutcome::cancelled);

    EXPECT_TRUE(cancelled);
    EXPECT_EQ(messages, successful_messages({"a"}));
    expect_committed("a");
    expect_source_retained("b");
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("b")));
}

TEST_F(HyperVBulkMigration, cancellationTakesPrecedenceOverEarlierRecoverableFailure)
{
    add_instance("a");
    add_instance("b");
    expect_state_query("a", "", "state query failed", 1);
    int cancellation_checks = 0;

    EXPECT_EQ(run([&] {
                  ++cancellation_checks;
                  return !messages.empty();
              }),
              mhv::MigrationOutcome::cancelled);

    EXPECT_EQ(cancellation_checks, 2);
    EXPECT_THAT(messages,
                ElementsAre(Pair(mhv::MigrationMessage::diagnostic,
                                 "Failed to migrate a: Could not query Hyper-V state for 'a': "
                                 "state query failed\n"),
                            Pair(mhv::MigrationMessage::summary, migration_summary({}))));
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("a")));
    EXPECT_FALSE(MP_FILEOPS.exists(target_dir("b")));
    EXPECT_FALSE(MP_FILEOPS.exists(target_root / "multipassd-vm-instances.json"));
    expect_source_retained("a");
    expect_source_retained("b");
}
