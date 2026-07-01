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

#include "../mock_virtual_machine.h"
#include "mock_hyperv_virtdisk_wrapper.h"
#include "tests/unit/common.h"

#include <hyperv_api/virtdisk/virtdisk_snapshot.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>

#include <QDir>

#include <filesystem>
#include <fstream>
#include <memory>

namespace multipass::test
{
namespace
{
namespace fs = std::filesystem;
using hyperv::OperationResult;
using hyperv::virtdisk::CreateVirtualDiskParameters;
using hyperv::virtdisk::VirtDiskSnapshot;

OperationResult op_ok()
{
    return OperationResult{S_OK, L""};
}

OperationResult op_fail()
{
    return OperationResult{E_FAIL, L"forced failure"};
}

void touch(const fs::path& p)
{
    std::ofstream ofs{p};
    ofs << "stub";
}
} // namespace

// These tests exercise the failure/rollback path of VirtDiskSnapshot::capture_impl().
// The VirtDisk wrapper is mocked so a disk-creation failure can be triggered
// deterministically, while the snapshot files themselves are real files in a
// temporary directory so the rollback's on-disk effects are actually verified.
struct VirtDiskSnapshotCapture : public ::testing::Test
{
    MockVirtDiskWrapper::GuardedMock virtdisk_injection =
        MockVirtDiskWrapper::inject<NiceMock>();
    MockVirtDiskWrapper& mock_virtdisk = *virtdisk_injection.first;

    NiceMock<MockVirtualMachine> vm;
    VirtualMachineDescription desc{};
    int snapshot_count = 0;

    void SetUp() override
    {
        desc.image.image_path = QDir(vm.tmp_dir->path()).filePath("base.vhdx").toStdString();
        ON_CALL(vm, get_snapshot_count()).WillByDefault([this] { return snapshot_count; });
    }

    fs::path dir() const
    {
        return fs::path{desc.image.image_path}.parent_path();
    }
    fs::path base() const
    {
        return desc.image.image_path;
    }
    fs::path head() const
    {
        return dir() / VirtDiskSnapshot::head_disk_name();
    }
    fs::path snapshot_path(int index) const
    {
        return dir() / fmt::format("{}.avhdx", index);
    }

    std::shared_ptr<VirtDiskSnapshot> make_snapshot(std::shared_ptr<Snapshot> parent = nullptr)
    {
        const VMSpecs specs{.num_cores = 1,
                            .mem_size = MemorySize{"1G"},
                            .disk_space = MemorySize{"5G"},
                            .default_mac_address = "00:00:00:00:00:00",
                            .extra_interfaces = {},
                            .ssh_username = "ubuntu",
                            .state = VirtualMachine::State::off,
                            .mounts = {},
                            .deleted = false,
                            .metadata = {}};
        return std::make_shared<VirtDiskSnapshot>("s0",
                                                  "",
                                                  "test-id",
                                                  std::move(parent),
                                                  specs,
                                                  vm,
                                                  desc);
    }
};

// A pre-existing head is renamed into the snapshot file, then creating the new
// head fails. The original head must be restored and the snapshot file removed.
TEST_F(VirtDiskSnapshotCapture, rolls_back_when_new_head_creation_fails)
{
    touch(head()); // the VM already has a live head disk

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_)).WillOnce(Return(op_fail()));

    auto ss = make_snapshot();
    EXPECT_THROW(ss->capture(), std::exception);

    EXPECT_TRUE(fs::exists(head())) << "the original head must be restored";
    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "the snapshot file must be cleaned up";
}

// No head exists yet (VM with no snapshots). The head is created during capture,
// renamed into the snapshot file, and then the new head creation fails. Rollback
// must leave neither a head nor a snapshot file behind (exact pre-capture state).
TEST_F(VirtDiskSnapshotCapture, rolls_back_when_no_preexisting_head)
{
    touch(base()); // base must exist so the head can be layered on it

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_))
        .WillOnce([](const CreateVirtualDiskParameters& params) {
            touch(params.path); // simulate the head disk being created
            return op_ok();
        })
        .WillOnce(Return(op_fail())); // new head creation fails

    auto ss = make_snapshot();
    EXPECT_THROW(ss->capture(), std::exception);

    EXPECT_FALSE(fs::exists(head())) << "no head should remain after a failed first capture";
    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "no snapshot file should remain";
}

} // namespace multipass::test
