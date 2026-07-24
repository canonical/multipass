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

#include <hyperv_api/virtdisk/virtdisk_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_snapshot.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>

#include <fmt/format.h>

#include <QDir>

#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

namespace multipass::test
{
namespace
{
namespace fs = std::filesystem;
using hyperv::OperationResult;
using hyperv::virtdisk::CreateVirtualDiskParameters;
using hyperv::virtdisk::CreateVirtdiskSnapshotError;
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
    MockVirtDiskWrapper::GuardedMock virtdisk_injection = MockVirtDiskWrapper::inject<NiceMock>();
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
    fs::path live_disk() const
    {
        return desc.image.image_path;
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
    fs::path new_live_disk() const
    {
        auto p = live_disk();
        p.replace_extension(".new.vhdx");
        return p;
    }

    fs::path old_live_disk() const
    {
        auto p = live_disk();
        p.replace_extension(".old.vhdx");
        return p;
    }
};

// The live disk is renamed into the snapshot file before its replacement is created.
// A creation failure must restore the original disk and remove the snapshot file.
TEST_F(VirtDiskSnapshotCapture, rolls_back_when_new_live_disk_creation_fails)
{
    touch(live_disk());

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_)).WillOnce(Return(op_fail()));

    auto ss = make_snapshot();
    EXPECT_THROW(ss->capture(), CreateVirtdiskSnapshotError);

    EXPECT_TRUE(fs::exists(live_disk())) << "the original live disk must be restored";
    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "the snapshot file must be cleaned up";
}

TEST_F(VirtDiskSnapshotCapture, rejects_missing_live_disk)
{
    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_)).Times(0);

    auto ss = make_snapshot();
    EXPECT_THROW(ss->capture(), CreateVirtdiskSnapshotError);

    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "no snapshot file should be created";
}

TEST_F(VirtDiskSnapshotCapture, rejects_existing_snapshot_disk_without_touching_live_disk)
{
    touch(live_disk());
    touch(snapshot_path(1));

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_)).Times(0);

    auto ss = make_snapshot();
    EXPECT_THROW(ss->capture(), CreateVirtdiskSnapshotError);

    EXPECT_TRUE(fs::exists(live_disk())) << "the live disk must remain intact";
    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "the existing snapshot disk must remain intact";
}

TEST_F(VirtDiskSnapshotCapture, creates_replacement_live_disk)
{
    touch(live_disk());

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_))
        .WillOnce([](const CreateVirtualDiskParameters& params) {
            EXPECT_EQ(std::get<hyperv::virtdisk::ParentPathParameters>(params.predecessor.get()).path,
                      params.path.parent_path() / "1.avhdx");
            touch(params.path);
            return op_ok();
        });

    auto ss = make_snapshot();
    EXPECT_NO_THROW(ss->capture());

    EXPECT_TRUE(fs::exists(live_disk()));
    EXPECT_TRUE(fs::exists(snapshot_path(1)));
}

// The snapshot file to restore is missing, so building the replacement disk fails
// before the live disk is touched.
TEST_F(VirtDiskSnapshotApply, apply_keeps_live_disk_when_snapshot_missing)
{
    touch(live_disk());
    // note: snapshot_path(1) is intentionally absent

    auto ss = make_snapshot();
    EXPECT_THROW(ss->apply(), std::exception);

    EXPECT_TRUE(fs::exists(live_disk())) << "the existing live disk must remain intact";
    EXPECT_FALSE(fs::exists(new_live_disk())) << "no temporary disk should be left behind";
}

TEST_F(VirtDiskSnapshotApply, apply_keeps_live_disk_when_replacement_creation_fails)
{
    touch(live_disk());
    touch(snapshot_path(1)); // snapshot to restore exists

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_)).WillOnce(Return(op_fail()));

    auto ss = make_snapshot();
    EXPECT_THROW(ss->apply(), std::exception);

    EXPECT_TRUE(fs::exists(live_disk())) << "the existing live disk must remain intact";
    EXPECT_FALSE(fs::exists(new_live_disk())) << "no temporary disk should be left behind";
    EXPECT_FALSE(fs::exists(old_live_disk())) << "no backup disk should be left behind";
}

TEST_F(VirtDiskSnapshotApply, apply_swaps_in_new_live_disk)
{
    touch(live_disk());
    touch(snapshot_path(1));

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_))
        .WillOnce([](const CreateVirtualDiskParameters& params) {
            touch(params.path);
            return op_ok();
        });

    auto ss = make_snapshot();
    EXPECT_NO_THROW(ss->apply());

    EXPECT_TRUE(fs::exists(live_disk())) << "the replacement live disk must be present";
    EXPECT_FALSE(fs::exists(new_live_disk())) << "the temporary disk must be renamed away";
    EXPECT_FALSE(fs::exists(old_live_disk())) << "the old live disk must be removed";
}

TEST_F(VirtDiskSnapshotApply, apply_restores_live_disk_when_swap_fails)
{
    touch(live_disk());
    touch(snapshot_path(1));

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_))
        .WillOnce([](const CreateVirtualDiskParameters& params) {
            touch(params.path);
            return op_ok();
        });

    auto fops_injection = MockFileOps::inject<NiceMock>();
    auto& fops = *fops_injection.first;

    ON_CALL(fops, exists(A<const fs::path&>())).WillByDefault([](const fs::path& p) {
        return std::filesystem::exists(p);
    });
    ON_CALL(fops, remove(A<const fs::path&>())).WillByDefault([](const fs::path& p) {
        return std::filesystem::remove(p);
    });
    ON_CALL(fops, remove(_, _)).WillByDefault([](const fs::path& p, std::error_code& ec) {
        return std::filesystem::remove(p, ec);
    });
    EXPECT_CALL(fops, rename(A<const fs::path&>(), A<const fs::path&>()))
        .Times(AnyNumber())
        .WillRepeatedly(
            [](const fs::path& from, const fs::path& to) { std::filesystem::rename(from, to); });
    EXPECT_CALL(fops, rename(new_live_disk(), live_disk()))
        .WillOnce(Throw(
            std::filesystem::filesystem_error{"forced failure",
                                              std::make_error_code(std::errc::permission_denied)}));

    auto ss = make_snapshot();
    EXPECT_THROW(ss->apply(), std::filesystem::filesystem_error);

    EXPECT_TRUE(fs::exists(live_disk())) << "the original live disk must be restored";
    EXPECT_FALSE(fs::exists(new_live_disk()));
    EXPECT_FALSE(fs::exists(old_live_disk()));
}

TEST_F(VirtDiskSnapshotApply, apply_removes_replacement_when_live_disk_cannot_be_moved)
{
    touch(live_disk());
    touch(snapshot_path(1));

    EXPECT_CALL(mock_virtdisk, create_virtual_disk(_))
        .WillOnce([](const CreateVirtualDiskParameters& params) {
            touch(params.path);
            return op_ok();
        });

    auto fops_injection = MockFileOps::inject<NiceMock>();
    auto& fops = *fops_injection.first;

    ON_CALL(fops, exists(A<const fs::path&>())).WillByDefault([](const fs::path& p) {
        return std::filesystem::exists(p);
    });
    ON_CALL(fops, remove(A<const fs::path&>())).WillByDefault([](const fs::path& p) {
        return std::filesystem::remove(p);
    });
    ON_CALL(fops, remove(_, _)).WillByDefault([](const fs::path& p, std::error_code& ec) {
        return std::filesystem::remove(p, ec);
    });
    EXPECT_CALL(fops, rename(live_disk(), old_live_disk()))
        .WillOnce(Throw(
            std::filesystem::filesystem_error{"forced failure",
                                              std::make_error_code(std::errc::permission_denied)}));

    auto ss = make_snapshot();
    EXPECT_THROW(ss->apply(), std::filesystem::filesystem_error);

    EXPECT_TRUE(fs::exists(live_disk())) << "the original live disk must remain intact";
    EXPECT_FALSE(fs::exists(new_live_disk())) << "the staged replacement must be removed";
    EXPECT_FALSE(fs::exists(old_live_disk())) << "no backup disk should be created";
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

        // No snapshot children: get_disk_children() iterates the snapshots and finds
        // nothing, so the live disk is the only child sitting on self.
        ON_CALL(vm, view_snapshots(_)).WillByDefault(Return(VirtualMachine::SnapshotVista{}));

        // Report every disk as parented on self, so get_disk_children() sees the live disk
        // attached to this snapshot.
        ON_CALL(mock_virtdisk, list_virtual_disk_chain(_, _, _))
            .WillByDefault([this](const fs::path& p,
                                  std::vector<fs::path>& chain,
                                  std::optional<std::size_t>) {
                chain.clear();
                chain.push_back(p);
                chain.push_back(snapshot_path(1)); // parent = self
                return op_ok();
            });

        // Used only by capture() to lay down the replacement live disk.
        ON_CALL(mock_virtdisk, create_virtual_disk(_))
            .WillByDefault([](const CreateVirtualDiskParameters& params) {
                touch(params.path);
                return op_ok();
            });
    }

    // Capture the snapshot: renames the live disk to self and creates its replacement.
    // snapshot into the "captured" state so it can be erased.
    std::shared_ptr<VirtDiskSnapshot> take_captured()
    {
        touch(live_disk());
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
    fs::path live_new() const
    {
        auto p = live_disk();
        p += ".new";
        return p;
    }
    fs::path live_old() const
    {
        auto p = live_disk();
        p += ".old";
        return p;
    }
};

// A merge fails during Pass 1. Nothing has been committed, so the pre-erase state
// must be fully restored: self back in place, live disk untouched, no sidecar files.
TEST_F(VirtDiskSnapshotErase, rolls_back_when_merge_fails)
{
    auto ss = take_captured();

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).WillOnce(Return(op_fail()));

    EXPECT_THROW(ss->erase(), std::exception);

    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must be restored";
    EXPECT_TRUE(fs::exists(live_disk())) << "the live disk must be left untouched";
    EXPECT_FALSE(fs::exists(self_tmp()));
    EXPECT_FALSE(fs::exists(live_new()));
    EXPECT_FALSE(fs::exists(live_old()));
}

// If the live disk exists but its chain cannot be inspected, erasing its possible
// parent must fail rather than orphaning the VM's active differencing disk.
TEST_F(VirtDiskSnapshotErase, throws_when_live_disk_chain_cannot_be_inspected)
{
    auto ss = take_captured();

    EXPECT_CALL(mock_virtdisk, list_virtual_disk_chain(live_disk(), _, _))
        .WillOnce(Return(op_fail()));
    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).Times(0);

    EXPECT_THROW(ss->erase(), std::exception);

    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must not be erased";
    EXPECT_TRUE(fs::exists(live_disk())) << "the unreadable live disk must remain";
    EXPECT_FALSE(fs::exists(self_tmp()));
}

// The happy path: the live disk is merged forward and committed. Self is gone and no
// sidecar files (.tmp / .new / .old) are left behind.
TEST_F(VirtDiskSnapshotErase, commits_and_cleans_up)
{
    auto ss = take_captured();

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).WillOnce(Return(op_ok()));

    EXPECT_NO_THROW(ss->erase());

    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "the erased snapshot must be gone";
    EXPECT_TRUE(fs::exists(live_disk())) << "the rebuilt live disk must be present";
    EXPECT_FALSE(fs::exists(self_tmp()));
    EXPECT_FALSE(fs::exists(live_new()));
    EXPECT_FALSE(fs::exists(live_old()));
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
    // live disk, which fails mid-commit (after the original was moved aside to .old).
    EXPECT_CALL(fops, rename(A<const fs::path&>(), A<const fs::path&>()))
        .Times(AnyNumber())
        .WillRepeatedly(
            [](const fs::path& a, const fs::path& b) { std::filesystem::rename(a, b); });
    EXPECT_CALL(fops, rename(live_new(), live_disk()))
        .WillOnce(Throw(
            std::filesystem::filesystem_error{"forced failure",
                                              std::make_error_code(std::errc::permission_denied)}));

    EXPECT_THROW(ss->erase(), std::exception);

    EXPECT_TRUE(fs::exists(live_disk())) << "the live disk must be restored from its backup";
    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must be restored";
    EXPECT_FALSE(fs::exists(self_tmp()));
    EXPECT_FALSE(fs::exists(live_new()));
    EXPECT_FALSE(fs::exists(live_old()));
}

// These tests exercise erase_impl()'s grandchild-reparenting pass. Deleting a snapshot
// rebuilds each of its direct children in place, which gives the child file a *new* VHDX
// identity; every grandchild therefore records a stale parent identity and must be
// re-linked onto its rebuilt parent via reparent_virtual_disk, or the differencing chain
// no longer validates when opened with its parents. The wrapper is mocked, so a
// multi-level chain is modelled by resolving each disk's parent from a lookup table.
struct VirtDiskSnapshotReparent : public VirtDiskSnapshotErase
{
    std::vector<std::shared_ptr<VirtDiskSnapshot>> model;
    std::map<fs::path, fs::path> parent_of; // immediate parent of each disk

    // Create a snapshot with a specific index (its file is `<index>.avhdx`) and add it
    // to the model returned by view_snapshots().
    std::shared_ptr<VirtDiskSnapshot> add(int index, std::shared_ptr<Snapshot> parent)
    {
        snapshot_count = index - 1; // BaseSnapshot index == get_snapshot_count() + 1
        auto ss = make_snapshot(std::move(parent));
        model.push_back(ss);
        return ss;
    }

    void SetUp() override
    {
        VirtDiskSnapshotErase::SetUp();

        ON_CALL(vm, view_snapshots(_)).WillByDefault([this](auto pred) {
            VirtualMachine::SnapshotVista result;
            for (const auto& s : model)
                if (!pred || pred(*s))
                    result.push_back(s);
            return result;
        });

        // Resolve each disk's immediate parent from parent_of; anything not listed is
        // reported as unrelated to the snapshot being erased.
        ON_CALL(mock_virtdisk, list_virtual_disk_chain(_, _, _))
            .WillByDefault([this](const fs::path& p,
                                  std::vector<fs::path>& chain,
                                  std::optional<std::size_t>) {
                chain.clear();
                chain.push_back(p);
                const auto it = parent_of.find(p);
                chain.push_back(it != parent_of.end() ? it->second : live_disk());
                return op_ok();
            });
    }

    void build_self_child_grandchildren()
    {
        touch(live_disk());
        add(1, nullptr);                 // s1: self, to be erased
        auto s2 = add(2, model.front()); // s2: direct child of self
        add(3, s2);                      // s3: grandchild (child of s2)
        parent_of[snapshot_path(2)] = snapshot_path(1);
        parent_of[snapshot_path(3)] = snapshot_path(2);
        model.front()->capture();
        touch(snapshot_path(2));
        touch(snapshot_path(3));
    }

    void expect_no_sidecars(int index) const
    {
        auto tmp = snapshot_path(index);
        tmp += ".tmp";
        EXPECT_FALSE(fs::exists(tmp)) << "leftover .tmp for " << index;
        auto nw = snapshot_path(index);
        nw += ".new";
        EXPECT_FALSE(fs::exists(nw)) << "leftover .new for " << index;
        auto old = snapshot_path(index);
        old += ".old";
        EXPECT_FALSE(fs::exists(old)) << "leftover .old for " << index;
    }
};

// Erasing a snapshot that has a grandchild rebuilds the direct child in place and then
// reparents the grandchild onto it. The unrelated live disk is not reparented.
TEST_F(VirtDiskSnapshotReparent, reparents_grandchild_onto_rebuilt_child)
{
    build_self_child_grandchildren();

    // The single direct child (s2) is merged into a rebuilt copy of self.
    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(snapshot_path(2)))
        .WillOnce(Return(op_ok()));

    // Only the grandchild (s3) is reparented, and only onto the rebuilt child (s2).
    EXPECT_CALL(mock_virtdisk, reparent_virtual_disk(_, _)).Times(0);
    EXPECT_CALL(mock_virtdisk, reparent_virtual_disk(snapshot_path(3), snapshot_path(2)))
        .WillOnce(Return(op_ok()));

    EXPECT_NO_THROW(model.front()->erase());

    EXPECT_FALSE(fs::exists(snapshot_path(1))) << "the erased snapshot must be gone";
    EXPECT_TRUE(fs::exists(snapshot_path(2))) << "the rebuilt child must remain";
    EXPECT_TRUE(fs::exists(snapshot_path(3))) << "the grandchild must remain";
    expect_no_sidecars(1);
    expect_no_sidecars(2);
}

// A snapshot child whose chain cannot be inspected must fail the erase instead of
// being silently skipped, because deleting its parent would leave the child broken.
TEST_F(VirtDiskSnapshotReparent, throws_when_snapshot_child_chain_cannot_be_inspected)
{
    build_self_child_grandchildren();

    EXPECT_CALL(mock_virtdisk, list_virtual_disk_chain(snapshot_path(2), _, _))
        .WillOnce(Return(op_fail()));
    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(_)).Times(0);
    EXPECT_CALL(mock_virtdisk, reparent_virtual_disk(_, _)).Times(0);

    EXPECT_THROW(model.front()->erase(), std::exception);

    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must not be erased";
    EXPECT_TRUE(fs::exists(snapshot_path(2))) << "the unreadable child must remain";
    EXPECT_TRUE(fs::exists(snapshot_path(3))) << "the grandchild must remain";
}

// If refreshing a grandchild's parent linkage fails, the whole erase aborts and rolls
// back: self and the child are restored and no sidecar files linger.
TEST_F(VirtDiskSnapshotReparent, rolls_back_when_reparent_fails)
{
    build_self_child_grandchildren();

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(snapshot_path(2)))
        .WillOnce(Return(op_ok()));
    EXPECT_CALL(mock_virtdisk, reparent_virtual_disk(snapshot_path(3), snapshot_path(2)))
        .WillOnce(Return(op_fail()));

    EXPECT_THROW(model.front()->erase(), std::exception);

    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must be restored";
    EXPECT_TRUE(fs::exists(snapshot_path(2))) << "the child must be restored";
    EXPECT_TRUE(fs::exists(snapshot_path(3))) << "the grandchild must remain";
    expect_no_sidecars(1);
    expect_no_sidecars(2);
}

// When a later grandchild's reparent fails after an earlier one already succeeded, the
// rollback must re-link the already-reparented grandchild back onto the restored
// original child (otherwise it would be left pointing at the discarded rebuilt identity).
// The earlier grandchild is therefore reparented twice: once forward, once on rollback.
TEST_F(VirtDiskSnapshotReparent, rollback_relinks_already_reparented_grandchildren)
{
    touch(live_disk());
    add(1, nullptr);                 // s1: self
    auto s2 = add(2, model.front()); // s2: direct child
    add(3, s2);                      // s3: grandchild #1 (reparented first, succeeds)
    add(4, s2);                      // s4: grandchild #2 (reparented second, fails)
    parent_of[snapshot_path(2)] = snapshot_path(1);
    parent_of[snapshot_path(3)] = snapshot_path(2);
    parent_of[snapshot_path(4)] = snapshot_path(2);
    model.front()->capture();
    touch(snapshot_path(2));
    touch(snapshot_path(3));
    touch(snapshot_path(4));

    EXPECT_CALL(mock_virtdisk, merge_virtual_disk_into_parent(snapshot_path(2)))
        .WillOnce(Return(op_ok()));
    // s3 reparented twice (forward + rollback re-link); s4's forward reparent fails.
    EXPECT_CALL(mock_virtdisk, reparent_virtual_disk(snapshot_path(3), snapshot_path(2)))
        .Times(2)
        .WillRepeatedly(Return(op_ok()));
    EXPECT_CALL(mock_virtdisk, reparent_virtual_disk(snapshot_path(4), snapshot_path(2)))
        .WillOnce(Return(op_fail()));

    EXPECT_THROW(model.front()->erase(), std::exception);

    EXPECT_TRUE(fs::exists(snapshot_path(1))) << "self must be restored";
    EXPECT_TRUE(fs::exists(snapshot_path(2))) << "the child must be restored";
    EXPECT_TRUE(fs::exists(snapshot_path(3)));
    EXPECT_TRUE(fs::exists(snapshot_path(4)));
    expect_no_sidecars(1);
    expect_no_sidecars(2);
}

} // namespace multipass::test
