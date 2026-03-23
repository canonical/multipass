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
#include "hyperv_test_utils.h"
#include "tests/unit/common.h"

#include <hyperv_api/virtdisk/virtdisk_snapshot.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>

namespace multipass::test
{

using namespace hyperv::virtdisk;
using Snap = std::shared_ptr<VirtDiskSnapshot>;
using Path = std::filesystem::path;

struct VirtDiskSnapshotErase : public ::testing::Test
{
    static constexpr auto vhdx_size = 1024ULL * 1024 * 16;

    NiceMock<MockVirtualMachine> vm;
    VirtualMachineDescription desc{};
    std::vector<Snap> snapshots;
    int counter = 0;

    void SetUp() override
    {
        auto bp = QDir(vm.tmp_dir->path()).filePath("base.vhdx");
        ASSERT_TRUE(VirtDisk().create_virtual_disk(
            {.size_in_bytes = vhdx_size, .path = bp.toStdString(), .predecessor = {}}));
        desc.image.image_path = bp;

        ON_CALL(vm, view_snapshots(_)).WillByDefault([this](auto pred) {
            VirtualMachine::SnapshotVista result;
            for (const auto& s : snapshots)
                if (!pred || pred(*s))
                    result.push_back(s);
            return result;
        });
        ON_CALL(vm, get_num_snapshots()).WillByDefault([this] {
            return static_cast<int>(snapshots.size());
        });
        ON_CALL(vm, get_snapshot_count()).WillByDefault([this] { return counter; });
    }

    // Create a snapshot and immediately capture it.
    Snap take(const std::string& name, std::shared_ptr<Snapshot> parent = nullptr)
    {
        VMSpecs specs{.num_cores = 1,
                      .mem_size = MemorySize{"1G"},
                      .disk_space = MemorySize{"5G"},
                      .default_mac_address = "00:00:00:00:00:00",
                      .extra_interfaces = {},
                      .ssh_username = "ubuntu",
                      .state = VirtualMachine::State::off,
                      .mounts = {},
                      .deleted = false,
                      .metadata = {}};
        ++counter;
        auto ss = std::make_shared<VirtDiskSnapshot>(name,
                                                     "",
                                                     "test-id",
                                                     std::move(parent),
                                                     specs,
                                                     vm,
                                                     desc);
        snapshots.push_back(ss);
        ss->capture();
        return ss;
    }

    // Remove from model and erase on disk.
    void drop(Snap& ss)
    {
        std::erase(snapshots, ss);
        ss->erase();
    }

    Path base() const
    {
        return desc.image.image_path.toStdString();
    }
    Path head() const
    {
        return base().parent_path() / VirtDiskSnapshot::head_disk_name();
    }
    Path snapshot_path(const Snapshot& ss) const
    {
        return base().parent_path() / VirtDiskSnapshot::make_snapshot_filename(ss);
    }

    void expect_chain(const Path& disk, const std::vector<Path>& expected)
    {
        std::vector<Path> chain;
        ASSERT_TRUE(VirtDisk().list_virtual_disk_chain(disk, chain))
            << "Failed to list chain for " << disk;
        ASSERT_EQ(chain.size(), expected.size()) << "Chain length mismatch for " << disk;
        for (size_t i = 0; i < expected.size(); ++i)
            EXPECT_TRUE(std::filesystem::equivalent(chain[i], expected[i]))
                << "chain[" << i << "]: " << chain[i] << " != " << expected[i];
    }

    void expect_gone(const Path& path)
    {
        EXPECT_FALSE(std::filesystem::exists(path)) << "Expected gone: " << path;
        auto tmp = path;
        tmp += ".tmp";
        EXPECT_FALSE(std::filesystem::exists(tmp)) << "Leftover .tmp: " << tmp;
    }

    void expect_gone(const Snap& ss)
    {
        expect_gone(snapshot_path(*ss));
    }
};

// ---------------------------------------------------------------------------
// 1. Leaf, no children, head elsewhere
// Merge loop is empty. Just removes the file.
// base <- s0 <- s1 <- head, restore s0, erase s1
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, leaf_no_children)
{
    auto s0 = take("s0");
    auto s1 = take("s1", s0);

    s0->apply();
    drop(s1);

    expect_gone(s1);
    expect_chain(head(), {head(), snapshot_path(*s0), base()});
}

// ---------------------------------------------------------------------------
// 2. Single child forward-merge + grandchild head survival
// One merge iteration. Head is grandchild via s2.
// base <- s0 <- s1 <- s2 <- head, erase s1
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, single_child_forward_merge)
{
    auto s0 = take("s0");
    auto s1 = take("s1", s0);
    auto s2 = take("s2", s1);

    s2->set_parent(s0);
    drop(s1);

    expect_gone(s1);
    expect_chain(snapshot_path(*s2), {snapshot_path(*s2), snapshot_path(*s0), base()});
    // If this fails, the TODO in erase_impl (grandchild reparenting) is needed.
    expect_chain(head(), {head(), snapshot_path(*s2), snapshot_path(*s0), base()});
}

// ---------------------------------------------------------------------------
// 3. Only snapshot, head attached (last snapshot deleted)
// Head is the only child. parent_path = base.
// base <- s0 <- head, erase s0
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, only_snapshot_head_attached)
{
    auto s0 = take("s0");

    drop(s0);

    expect_gone(s0);
    expect_chain(head(), {head(), base()});
}

// ---------------------------------------------------------------------------
// 4. Root deletion with snapshot child (parent_path = base fallback)
// base <- s0 <- s1 <- head, erase s0
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, root_deletion)
{
    auto s0 = take("s0");
    auto s1 = take("s1", s0);

    s1->set_parent(nullptr);
    drop(s0);

    expect_gone(s0);
    expect_chain(snapshot_path(*s1), {snapshot_path(*s1), base()});
    expect_chain(head(), {head(), snapshot_path(*s1), base()});
}

// ---------------------------------------------------------------------------
// 5. Two snapshot children + head (multi-iteration loop, head as grandchild)
// base <- s0 <- s1 <- s2
//                   <- s1b <- head
// erase s1
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, two_children_and_head)
{
    auto s0 = take("s0");
    auto s1 = take("s1", s0);
    auto s2 = take("s2", s1);

    s1->apply();
    auto s1b = take("s1b", s1);

    s2->set_parent(s0);
    s1b->set_parent(s0);
    drop(s1);

    expect_gone(s1);
    expect_chain(snapshot_path(*s2), {snapshot_path(*s2), snapshot_path(*s0), base()});
    expect_chain(snapshot_path(*s1b), {snapshot_path(*s1b), snapshot_path(*s0), base()});
    expect_chain(head(), {head(), snapshot_path(*s1b), snapshot_path(*s0), base()});
}

// ---------------------------------------------------------------------------
// 6. Snapshot child + head both directly attached to deleted snapshot
// base <- s0 <- s1 <- s2 (on disk)
//                   <- head (after restore s1)
// erase s1
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, children_and_head_both_attached)
{
    auto s0 = take("s0");
    auto s1 = take("s1", s0);
    auto s2 = take("s2", s1);

    s1->apply();

    s2->set_parent(s0);
    drop(s1);

    expect_gone(s1);
    expect_chain(snapshot_path(*s2), {snapshot_path(*s2), snapshot_path(*s0), base()});
    expect_chain(head(), {head(), snapshot_path(*s0), base()});
}

// ---------------------------------------------------------------------------
// 7. Multi-level multi-branch: build, collapse in phases, apply all, teardown
//
// Initial tree (8 snapshots, 4 levels, branching at s1 and s2):
//   base <- s0 <- s1 <- s2 <- s3
//                          <- s2b
//                    <- s1b <- s1b1
//                           <- s1b2 <- head
//
// Phase 1: erase s2   (2 children: s3, s2b)
// Phase 2: erase s1b  (2 children: s1b1, s1b2; head is grandchild via s1b2)
// Phase 3: erase s1   (4 children: s3, s2b, s1b1, s1b2; head via s1b2)
// Phase 4: apply every survivor to confirm valid heads
// Phase 5: delete all leaves, then root -> base <- head
// ---------------------------------------------------------------------------
TEST_F(VirtDiskSnapshotErase, multi_level_full_lifecycle)
{
    // Build tree
    auto s0 = take("s0");
    auto s1 = take("s1", s0);
    auto s2 = take("s2", s1);
    auto s3 = take("s3", s2);

    s2->apply();
    auto s2b = take("s2b", s2);

    s1->apply();
    auto s1b = take("s1b", s1);
    auto s1b1 = take("s1b1", s1b);

    s1b->apply();
    auto s1b2 = take("s1b2", s1b);

    // Verify initial structure
    expect_chain(
        snapshot_path(*s3),
        {snapshot_path(*s3), snapshot_path(*s2), snapshot_path(*s1), snapshot_path(*s0), base()});
    expect_chain(
        snapshot_path(*s2b),
        {snapshot_path(*s2b), snapshot_path(*s2), snapshot_path(*s1), snapshot_path(*s0), base()});
    expect_chain(snapshot_path(*s1b1),
                 {snapshot_path(*s1b1),
                  snapshot_path(*s1b),
                  snapshot_path(*s1),
                  snapshot_path(*s0),
                  base()});
    expect_chain(snapshot_path(*s1b2),
                 {snapshot_path(*s1b2),
                  snapshot_path(*s1b),
                  snapshot_path(*s1),
                  snapshot_path(*s0),
                  base()});
    expect_chain(head(),
                 {head(),
                  snapshot_path(*s1b2),
                  snapshot_path(*s1b),
                  snapshot_path(*s1),
                  snapshot_path(*s0),
                  base()});

    // Phase 1: erase s2
    s3->set_parent(s1);
    s2b->set_parent(s1);
    drop(s2);

    expect_gone(s2);
    expect_chain(snapshot_path(*s3),
                 {snapshot_path(*s3), snapshot_path(*s1), snapshot_path(*s0), base()});
    expect_chain(snapshot_path(*s2b),
                 {snapshot_path(*s2b), snapshot_path(*s1), snapshot_path(*s0), base()});
    expect_chain(head(),
                 {head(),
                  snapshot_path(*s1b2),
                  snapshot_path(*s1b),
                  snapshot_path(*s1),
                  snapshot_path(*s0),
                  base()});

    // Phase 2: erase s1b (head is grandchild via s1b2)
    s1b1->set_parent(s1);
    s1b2->set_parent(s1);
    drop(s1b);

    expect_gone(s1b);
    expect_chain(snapshot_path(*s1b1),
                 {snapshot_path(*s1b1), snapshot_path(*s1), snapshot_path(*s0), base()});
    expect_chain(snapshot_path(*s1b2),
                 {snapshot_path(*s1b2), snapshot_path(*s1), snapshot_path(*s0), base()});
    expect_chain(head(),
                 {head(), snapshot_path(*s1b2), snapshot_path(*s1), snapshot_path(*s0), base()});

    // Phase 3: erase s1 (4 snapshot children + head via s1b2)
    s3->set_parent(s0);
    s2b->set_parent(s0);
    s1b1->set_parent(s0);
    s1b2->set_parent(s0);
    drop(s1);

    expect_gone(s1);
    for (const auto& ss : {s3, s2b, s1b1, s1b2})
        expect_chain(snapshot_path(*ss), {snapshot_path(*ss), snapshot_path(*s0), base()});
    expect_chain(head(), {head(), snapshot_path(*s1b2), snapshot_path(*s0), base()});

    // Phase 4: apply every survivor
    s3->apply();
    expect_chain(head(), {head(), snapshot_path(*s3), snapshot_path(*s0), base()});

    s2b->apply();
    expect_chain(head(), {head(), snapshot_path(*s2b), snapshot_path(*s0), base()});

    s1b1->apply();
    expect_chain(head(), {head(), snapshot_path(*s1b1), snapshot_path(*s0), base()});

    s1b2->apply();
    expect_chain(head(), {head(), snapshot_path(*s1b2), snapshot_path(*s0), base()});

    s0->apply();
    expect_chain(head(), {head(), snapshot_path(*s0), base()});

    // Phase 5: delete all leaves, then root
    for (auto leaf : {s3, s2b, s1b1, s1b2})
        drop(leaf);

    expect_chain(head(), {head(), snapshot_path(*s0), base()});

    drop(s0);
    expect_chain(head(), {head(), base()});
}

} // namespace multipass::test
