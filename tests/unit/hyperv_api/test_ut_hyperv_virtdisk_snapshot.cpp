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

#include "../mock_file_ops.h"
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

// These tests exercise the failure/rollback path of VirtDiskSnapshot::capture_impl()
// and apply_impl(). The VirtDisk wrapper is mocked so a disk-creation failure can
// be triggered deterministically, while the snapshot files themselves are real
// files in a temporary directory so the on-disk effects are actually verified.
struct VirtDiskSnapshotTest : public ::testing::Test
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

struct VirtDiskSnapshotCapture : public VirtDiskSnapshotTest
{
};

struct VirtDiskSnapshotApply : public VirtDiskSnapshotTest
{
    fs::path new_head() const
    {
        auto p = head();
        p.replace_extension(".new.avhdx");
        return p;
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

// The snapshot file to restore is missing, so building the replacement head
// fails before it starts. The live head must be left untouched (the VM stays
// bootable) and no temporary ".new" head should be left behind.
TEST_F(VirtDiskSnapshotApply, apply_keeps_head_when_snapshot_missing)
{
    touch(head()); // the VM's current head disk
    // note: snapshot_path(1) is intentionally absent

    auto ss = make_snapshot();
    EXPECT_THROW(ss->apply(), std::exception);

    EXPECT_TRUE(fs::exists(head())) << "the existing head must remain intact";
    EXPECT_FALSE(fs::exists(new_head())) << "no temporary head should be left behind";
}

// Building the replacement head fails (VirtDisk API error). Because the new head
// is built before the old one is swapped out, the existing head must survive.
TEST_F(VirtDiskSnapshotApply, apply_keeps_head_when_new_head_creation_fails)
{
    touch(head());
    touch(snapshot_path(1)); // snapshot to restore exists

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_)).WillOnce(Return(op_fail()));

    auto ss = make_snapshot();
    EXPECT_THROW(ss->apply(), std::exception);

    EXPECT_TRUE(fs::exists(head())) << "the existing head must remain intact";
    EXPECT_FALSE(fs::exists(new_head())) << "no temporary head should be left behind";
}

// The happy path: the replacement head is built from the snapshot and swapped in
// for the old head. No throw, a head remains, and no temporary ".new" head lingers.
TEST_F(VirtDiskSnapshotApply, apply_swaps_in_new_head)
{
    touch(head());
    touch(snapshot_path(1));

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_))
        .WillOnce([](const CreateVirtualDiskParameters& params) {
            touch(params.path); // simulate the new head disk being created
            return op_ok();
        });

    auto ss = make_snapshot();
    EXPECT_NO_THROW(ss->apply());

    EXPECT_TRUE(fs::exists(head())) << "the swapped-in head must be present";
    EXPECT_FALSE(fs::exists(new_head())) << "the temporary head must be renamed away";
}

// These tests exercise VirtDiskSnapshot::erase_impl()'s transactional behaviour.
// The simplest child set is used: this snapshot with the VM's head attached to it
// and no snapshot children (so view_snapshots is empty). The snapshot is first
// captured (creating real self + head files on disk and marking it "captured"),
// then erased. The VirtDisk wrapper is mocked so merges succeed/fail on demand.
struct VirtDiskSnapshotErase : public VirtDiskSnapshotTest
{
    void SetUp() override
    {
        VirtDiskSnapshotTest::SetUp();

        // No snapshot children: get_children() iterates this and finds nothing, so
        // the head (added via is_head_attached_to_this) is the only child.
        ON_CALL(vm, view_snapshots(_)).WillByDefault(Return(VirtualMachine::SnapshotVista{}));

        // Report every disk as parented on self, so is_head_attached_to_this() sees
        // the head attached to this snapshot.
        ON_CALL(mock_virtdisk, list_virtual_disk_chain(_, _, _))
            .WillByDefault([this](const fs::path& p,
                                  std::vector<fs::path>& chain,
                                  std::optional<std::size_t>) {
                chain.clear();
                chain.push_back(p);
                chain.push_back(snapshot_path(1)); // parent = self
                return op_ok();
            });

        // Used only by capture() to lay down real stub files for self and head.
        ON_CALL(mock_virtdisk, create_virtual_disk(_))
            .WillByDefault([](const CreateVirtualDiskParameters& params) {
                touch(params.path);
                return op_ok();
            });
    }

    // Capture the snapshot: creates real self (1.avhdx) + head files and puts the
    // snapshot into the "captured" state so it can be erased.
    std::shared_ptr<VirtDiskSnapshot> take_captured()
    {
        touch(base()); // capture layers the first snapshot on the base disk
        auto ss = make_snapshot();
        ss->capture();
        return ss;
    }

    fs::path self_tmp() const
    {
        auto p = snapshot_path(1);
        p += ".tmp";
        return p;
    }
    fs::path head_new() const
    {
        auto p = head();
        p += ".new";
        return p;
    }
    fs::path head_old() const
    {
        auto p = head();
        p += ".old";
        return p;
    }
};

// A merge fails during Pass 1. Nothing has been committed, so the pre-erase state
// must be fully restored: self back in place, head untouched, no sidecar files.
TEST_F(VirtDiskSnapshotErase, rolls_back_when_merge_fails)
{
    auto ss = take_captured();

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).WillOnce(Return(op_fail()));

    EXPECT_THROW(ss->erase(), std::exception);

    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must be restored";
    EXPECT_TRUE(fs::exists(head())) << "the head must be left untouched";
    EXPECT_FALSE(fs::exists(self_tmp()));
    EXPECT_FALSE(fs::exists(head_new()));
    EXPECT_FALSE(fs::exists(head_old()));
}

// The happy path: the head is merged forward and committed. Self is gone and no
// sidecar files (.tmp / .new / .old) are left behind.
TEST_F(VirtDiskSnapshotErase, commits_and_cleans_up)
{
    auto ss = take_captured();

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).WillOnce(Return(op_ok()));

    EXPECT_NO_THROW(ss->erase());

    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "the erased snapshot must be gone";
    EXPECT_TRUE(fs::exists(head())) << "the reparented head must be present";
    EXPECT_FALSE(fs::exists(self_tmp()));
    EXPECT_FALSE(fs::exists(head_new()));
    EXPECT_FALSE(fs::exists(head_old()));
}

// A file operation fails mid-commit (Pass 2). Everything except the final swap-in
// of the new head is delegated to the real filesystem; that one rename is forced
// to throw. The original head must be restored from its ".old" backup and self
// restored, with no stragglers.
TEST_F(VirtDiskSnapshotErase, rolls_back_when_commit_fails)
{
    auto ss = take_captured();

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).WillOnce(Return(op_ok()));

    auto fops_injection = MockFileOps::inject<NiceMock>();
    auto& fops = *fops_injection.first;

    ON_CALL(fops, exists(A<const fs::path&>())).WillByDefault([](const fs::path& p) {
        return std::filesystem::exists(p);
    });
    ON_CALL(fops, exists(_, _)).WillByDefault([](const fs::path& p, std::error_code& ec) {
        return std::filesystem::exists(p, ec);
    });
    ON_CALL(fops, remove(A<const fs::path&>())).WillByDefault([](const fs::path& p) {
        return std::filesystem::remove(p);
    });
    ON_CALL(fops, remove(_, _)).WillByDefault([](const fs::path& p, std::error_code& ec) {
        return std::filesystem::remove(p, ec);
    });
    ON_CALL(fops, copy(_, _, _))
        .WillByDefault([](const fs::path& s, const fs::path& d, fs::copy_options o) {
            std::filesystem::copy(s, d, o);
        });
    ON_CALL(fops, weakly_canonical(_)).WillByDefault([](const fs::path& p) {
        return std::filesystem::weakly_canonical(p);
    });

    // Delegate every rename to the real filesystem, except the swap-in of the new
    // head, which fails mid-commit (after the original was moved aside to .old).
    EXPECT_CALL(fops, rename(A<const fs::path&>(), A<const fs::path&>()))
        .Times(AnyNumber())
        .WillRepeatedly(
            [](const fs::path& a, const fs::path& b) { std::filesystem::rename(a, b); });
    EXPECT_CALL(fops, rename(head_new(), head()))
        .WillOnce(Throw(std::filesystem::filesystem_error{
            "forced failure",
            std::make_error_code(std::errc::permission_denied)}));

    EXPECT_THROW(ss->erase(), std::exception);

    EXPECT_TRUE(fs::exists(head())) << "the head must be restored from its .old backup";
    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must be restored";
    EXPECT_FALSE(fs::exists(self_tmp()));
    EXPECT_FALSE(fs::exists(head_new()));
    EXPECT_FALSE(fs::exists(head_old()));
}

} // namespace multipass::test
