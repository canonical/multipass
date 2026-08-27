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
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/temp_dir.h"

#include <src/platform/backends/hyperv/hcs_ownership.h>
#include <src/platform/backends/hyperv/hyperv_disk_layout.h>

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
using namespace testing;

namespace
{
void write_snapshot(const std::filesystem::path& path, int index)
{
    const boost::json::object snapshot{
        {"name", fmt::format("snapshot{}", index)},
        {"comment", ""},
        {"parent", 0},
        {"cloud_init_instance_id", "instance-id"},
        {"index", index},
        {"creation_timestamp", "2026-08-19T00:00:00.000Z"},
        {"num_cores", 1},
        {"mem_size", "1073741824"},
        {"disk_space", "5368709120"},
        {"extra_interfaces", boost::json::array{}},
        {"state", static_cast<int>(mp::VirtualMachine::State::stopped)},
        {"mounts", boost::json::array{}},
        {"metadata", boost::json::object{}}};
    const boost::json::object json{{"snapshot", snapshot}};
    MP_FILEOPS.write_transactionally(path, mp::pretty_print(json));
}
} // namespace

TEST(HCSOwnership, roundTrips)
{
    mpt::TempDir instance_dir;
    const mhv::HCSOwnership expected{
        .active_disk = instance_dir.filePath("active.avhdx").toStdString(),
        .state_file_stem = instance_dir.filePath("hcs-state").toStdString(),
    };

    expected.persist(instance_dir.path().toStdString());
    const auto actual = mhv::HCSOwnership::load(instance_dir.path().toStdString());

    ASSERT_TRUE(actual);
    EXPECT_EQ(actual->active_disk, expected.active_disk);
    EXPECT_EQ(actual->state_file_stem, expected.state_file_stem);
}

TEST(LegacyDiskLayout, writesExplicitSnapshotPaths)
{
    NiceMock<mpt::MockVirtualMachine> vm;
    const auto snapshot_metadata = std::filesystem::path{
        vm.tmp_dir->filePath("0001.snapshot.json").toStdString()};
    write_snapshot(snapshot_metadata, 1);

    const auto legacy_disk = vm.tmp_dir->filePath("legacy.avhdx").toStdString();
    const mhv::LegacyDiskLayout layout{.snapshots = {{.index = 1,
                                                      .checkpoint_name = "@s1",
                                                      .checkpoint_id = "checkpoint-id",
                                                      .disk_path = legacy_disk}}};

    layout.persist_snapshot_paths(vm);

    const auto contents = MP_FILEOPS.try_read_file(snapshot_metadata);
    ASSERT_TRUE(contents);
    const auto json = boost::json::parse(*contents);
    EXPECT_EQ(boost::json::value_to<std::string>(json.at("snapshot").at("disk_path")), legacy_disk);
}
