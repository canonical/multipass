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
#include "tests/unit/mock_snapshot.h"
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/windows/powershell_test_helper.h"

#include <src/platform/backends/hyperv/hyperv_disk_layout.h>

#include <multipass/json_utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
using namespace testing;

namespace
{
std::string discovery_json(
    const std::filesystem::path& active_disk,
    const std::vector<std::pair<std::string, std::filesystem::path>>& snapshots)
{
    boost::json::array snapshot_array;
    for (const auto& [name, path] : snapshots)
        snapshot_array.push_back({{"Name", name}, {"Id", name + "-id"}, {"Path", path.string()}});

    const boost::json::object result{{"ActiveDisk", active_disk.string()},
                                     {"Snapshots", std::move(snapshot_array)}};
    return boost::json::serialize(result);
}

std::shared_ptr<NiceMock<mpt::MockSnapshot>> snapshot(int index)
{
    auto result = std::make_shared<NiceMock<mpt::MockSnapshot>>();
    ON_CALL(*result, get_index()).WillByDefault(Return(index));
    ON_CALL(*result, get_name()).WillByDefault(Return("snapshot" + std::to_string(index)));
    ON_CALL(*result, get_parent()).WillByDefault(Return(nullptr));
    return result;
}
} // namespace

TEST(LegacyDiskLayout, rejectsDuplicateCheckpointDiskPaths)
{
    NiceMock<mpt::MockVirtualMachine> vm;
    const auto active = std::filesystem::path{vm.tmp_dir->filePath("active.avhdx").toStdString()};
    const auto duplicate = std::filesystem::path{
        vm.tmp_dir->filePath("duplicate.avhdx").toStdString()};
    mpt::make_file_with_content(QString::fromStdString(active.string()), "active");
    mpt::make_file_with_content(QString::fromStdString(duplicate.string()), "snapshot");

    const auto first = snapshot(1);
    const auto second = snapshot(2);
    ON_CALL(vm, view_snapshots(_))
        .WillByDefault(Return(mp::VirtualMachine::SnapshotVista{first, second}));

    mpt::PowerShellTestHelper powershell;
    powershell.setup_mocked_run_sequence(
        {{"Get-VMHardDiskDrive",
          discovery_json(active, {{"@s1", duplicate}, {"@s2", duplicate}})}});

    EXPECT_THROW((void)mhv::resolve_legacy_disk_layout("migration-test", vm), std::runtime_error);
}

TEST(LegacyDiskLayout, rejectsSnapshotsOnDifferentBaseImages)
{
    NiceMock<mpt::MockVirtualMachine> vm;
    const auto active = std::filesystem::path{vm.tmp_dir->filePath("active.avhdx").toStdString()};
    const auto first_disk = std::filesystem::path{
        vm.tmp_dir->filePath("base-a.vhdx").toStdString()};
    const auto second_disk = std::filesystem::path{
        vm.tmp_dir->filePath("snapshot-b.avhdx").toStdString()};
    const auto second_base = std::filesystem::path{
        vm.tmp_dir->filePath("base-b.vhdx").toStdString()};
    for (const auto& path : {active, first_disk, second_disk, second_base})
        mpt::make_file_with_content(QString::fromStdString(path.string()), "disk");
    mpt::make_file_with_content(vm.tmp_dir->filePath("snapshot-head"), "1\n");

    const auto first = snapshot(1);
    const auto second = snapshot(2);
    ON_CALL(vm, view_snapshots(_))
        .WillByDefault(Return(mp::VirtualMachine::SnapshotVista{first, second}));

    mpt::PowerShellTestHelper powershell;
    powershell.setup_mocked_run_sequence(
        {{"Get-VMHardDiskDrive",
          discovery_json(active, {{"@s1", first_disk}, {"@s2", second_disk}})}});

    auto virtdisk = mpt::MockVirtDiskWrapper::inject<NiceMock>();
    ON_CALL(*virtdisk.first, list_virtual_disk_chain(active, _, _))
        .WillByDefault(DoAll(SetArgReferee<1>(std::vector{active, first_disk}),
                             Return(mhv::OperationResult::success())));
    ON_CALL(*virtdisk.first, list_virtual_disk_chain(first_disk, _, _))
        .WillByDefault(DoAll(SetArgReferee<1>(std::vector{first_disk}),
                             Return(mhv::OperationResult::success())));
    ON_CALL(*virtdisk.first, list_virtual_disk_chain(second_disk, _, _))
        .WillByDefault(DoAll(SetArgReferee<1>(std::vector{second_disk, second_base}),
                             Return(mhv::OperationResult::success())));

    EXPECT_THROW((void)mhv::resolve_legacy_disk_layout("migration-test", vm), std::runtime_error);
}
