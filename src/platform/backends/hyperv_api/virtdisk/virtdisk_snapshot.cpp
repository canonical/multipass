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

#include <hyperv_api/virtdisk/virtdisk_snapshot.h>

#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/file_ops.h>
#include <multipass/logging/log.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <scope_guard.hpp>

namespace
{
constexpr auto log_category = "virtdisk-snapshot";
namespace mpl = multipass::logging;
} // namespace

namespace multipass::hyperv::virtdisk
{

struct CreateVirtdiskSnapshotError : FormattedExceptionBase<std::system_error>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

// Best-effort rename for use inside noexcept rollback guards: MP_FILEOPS.rename has
// no error_code overload, so a failure during cleanup is caught and logged rather
// than allowed to escape (and mask) the original error.
static void try_rename(const std::filesystem::path& from, const std::filesystem::path& to) noexcept
{
    try
    {
        MP_FILEOPS.rename(from, to);
    }
    catch (const std::exception& e)
    {
        mpl::error(log_category,
                   "Failed to restore `{}` -> `{}` during rollback: {}",
                   from,
                   to,
                   e.what());
    }
}

namespace
{
// Append a sidecar suffix to a disk path (e.g. `2.avhdx` -> `2.avhdx.new`), keeping the
// backup/scratch companions of a disk next to it and easy to spot.
std::filesystem::path with_suffix(std::filesystem::path p, const char* suffix)
{
    p += suffix;
    return p;
}

/**
 * Transactional rebuild of a snapshot's direct children when that snapshot is erased.
 *
 * Deleting snapshot S means every disk sitting directly on S must absorb S's contents so
 * it stops depending on S. Each such child is rebuilt in place from a copy of S; that
 * gives the rebuilt file a *new* VHDX identity, so S's grandchildren (which recorded the
 * child's *old* identity) are then relinked onto their rebuilt parent.
 *
 * The steps are staged so that any failure restores the exact pre-erase state:
 *   begin()    - move S aside as the pristine merge source ("<S>.tmp").
 *   stage()    - Pass 1: merge each child into a fresh copy of S, staged as "<child>.new";
 *                no original file is touched yet.
 *   commit()   - Pass 2: preserve each original as "<child>.old", then swap the staged
 *                result into place.
 *   reparent() - refresh each grandchild's stored parent identity onto its rebuilt child.
 *   finalize() - success: drop the "<child>.old" and "<S>.tmp" backups.
 *   rollback() - failure: restore originals and S, undoing any reparenting; never throws.
 *
 * begin() performs a filesystem side effect and must run *before* the caller arms a scope
 * guard around rollback(); the remaining steps run under that guard.
 */
class ChildRebuild
{
public:
    ChildRebuild(std::filesystem::path self_path,
                 std::vector<std::filesystem::path> children,
                 std::vector<std::pair<std::filesystem::path, std::filesystem::path>> grandchildren)
        : self_path{std::move(self_path)},
          self_backup{with_suffix(this->self_path, ".tmp")},
          children{std::move(children)},
          grandchildren{std::move(grandchildren)}
    {
        staged.reserve(this->children.size());
    }

    // Move self aside as the pristine merge source. Must precede arming the rollback.
    void begin()
    {
        MP_FILEOPS.rename(self_path, self_backup);
    }

    // Pass 1: merge each child into a fresh copy of self, staged as "<child>.new". No
    // original files are modified, so this leaves nothing to undo beyond the staged copies.
    void stage()
    {
        for (const auto& child_path : children)
        {
            auto staged_path = with_suffix(child_path, ".new");

            MP_FILEOPS.copy(self_backup, self_path, {}); // fresh parent for this merge

            if (auto merge_r = VirtDisk().merge_virtual_disk_into_parent(child_path); !merge_r)
                throw CreateVirtdiskSnapshotError{merge_r,
                                                  "Could not merge child disk `{}` into `{}`",
                                                  child_path,
                                                  self_path};

            MP_FILEOPS.rename(self_path, staged_path); // stash merged result
            staged.emplace_back(child_path, staged_path);
        }
    }

    // Pass 2: commit. Preserve each original as "<child>.old", then swap the merged result
    // in. Keeping the originals until the end lets rollback() restore them if a later swap
    // (or the reparent pass) fails.
    void commit()
    {
        for (const auto& [child_path, staged_path] : staged)
        {
            MP_FILEOPS.rename(child_path, with_suffix(child_path, ".old")); // preserve original
            MP_FILEOPS.rename(staged_path, child_path);                     // put merged in place
        }
    }

    // Refresh every grandchild's recorded parent identity to match its rebuilt child, whose
    // file identity changed in commit(). SetVirtualDiskInformation (via reparent_virtual_disk)
    // rewrites the grandchild's stored parent GUID/timestamp, which is what a differencing
    // disk validates against its parent when the chain is opened. A failure throws so the
    // caller's guard rolls the whole erase back rather than orphaning a grandchild.
    void reparent()
    {
        for (const auto& [grandchild, child_path] : grandchildren)
        {
            if (const auto r = VirtDisk().reparent_virtual_disk(grandchild, child_path); !r)
                throw CreateVirtdiskSnapshotError{r,
                                                  "Could not reparent `{}` onto rebuilt `{}`",
                                                  grandchild,
                                                  child_path};
            reparented.emplace_back(grandchild, child_path);
        }
    }

    // Success: drop the preserved originals and the self backup. Best-effort; nothing
    // references them anymore.
    void finalize() noexcept
    {
        std::error_code ec{};
        for (const auto& [child_path, staged_path] : staged)
            MP_FILEOPS.remove(with_suffix(child_path, ".old"), ec);
        MP_FILEOPS.remove(self_backup, ec);
    }

    // Restore the exact pre-erase state. Existence-driven so it is correct no matter how far
    // stage()/commit() got, and noexcept so it never masks the original error.
    void rollback() noexcept
    {
        std::error_code ec{};
        for (const auto& [child_path, staged_path] : staged)
        {
            const auto backup_path = with_suffix(child_path, ".old");
            if (MP_FILEOPS.exists(backup_path, ec)) // commit() had started for this child
            {
                MP_FILEOPS.remove(child_path, ec);   // drop merged/partial content
                try_rename(backup_path, child_path); // restore the original
            }
            MP_FILEOPS.remove(staged_path, ec); // drop the staged .new if still present
        }
        MP_FILEOPS.remove(self_path, ec);   // drop leftover merge scratch
        try_rename(self_backup, self_path); // restore pristine self

        // The original children are back at their paths with their original identity;
        // re-link any grandchild we had already reparented so its stored parent identity
        // matches again. Best-effort: a failure here is logged, not thrown.
        for (const auto& [grandchild, child_path] : reparented)
            if (const auto r = VirtDisk().reparent_virtual_disk(grandchild, child_path); !r)
                mpl::error(log_category,
                           "Failed to restore parent linkage of `{}` onto `{}` during rollback",
                           grandchild,
                           child_path);
    }

private:
    std::filesystem::path self_path;
    std::filesystem::path self_backup;
    std::vector<std::filesystem::path> children;

    // {grandchild, rebuilt-parent} pairs to relink after the children are rebuilt.
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> grandchildren;

    // {child, staged("<child>.new")} for every child stage() has processed.
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> staged;

    // Grandchildren already reparented; rollback() must relink them onto the restored
    // originals (which carry the original identity again).
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> reparented;
};
} // namespace

VirtDiskSnapshot::VirtDiskSnapshot(const std::string& name,
                                   const std::string& comment,
                                   const std::string& instance_id,
                                   std::shared_ptr<Snapshot> parent,
                                   const VMSpecs& specs,
                                   const VirtualMachine& vm,
                                   const VirtualMachineDescription& desc)
    : BaseSnapshot(name, comment, instance_id, std::move(parent), specs, vm),
      base_vhdx_path{desc.image.image_path},
      vm{vm}
{
}

VirtDiskSnapshot::VirtDiskSnapshot(const std::filesystem::path& filename,
                                   VirtualMachine& vm,
                                   const VirtualMachineDescription& desc)
    : BaseSnapshot(filename, vm, desc), base_vhdx_path{desc.image.image_path}, vm{vm}
{
}

std::string VirtDiskSnapshot::make_snapshot_filename(const Snapshot& ss)
{
    return fmt::format("{}.avhdx", ss.get_index());
}

std::filesystem::path VirtDiskSnapshot::make_snapshot_path(const Snapshot& ss) const
{
    return base_vhdx_path.parent_path() / make_snapshot_filename(ss);
}

std::filesystem::path VirtDiskSnapshot::make_head_disk_path() const
{
    return base_vhdx_path.parent_path() / head_disk_name();
}

void VirtDiskSnapshot::capture_impl()
{
    const auto head_path = make_head_disk_path();
    const auto snapshot_path = make_snapshot_path(*this);
    mpl::debug(log_category,
               "capture_impl() -> head_path: {}, snapshot_path: {}",
               head_path,
               snapshot_path);

    const bool head_preexisted = MP_FILEOPS.exists(head_path);
    bool head_became_snapshot = false;

    // Undo partial work on failure so a failed capture leaves no trace. Fires on
    // exception, dismissed on success; never throws so the original error wins.
    auto rollback = sg::make_scope_guard([&]() noexcept {
        std::error_code ec{};
        if (head_became_snapshot) // the live head was renamed away; put it back
        {
            MP_FILEOPS.remove(head_path, ec);
            try
            {
                MP_FILEOPS.rename(snapshot_path, head_path);
            }
            catch (const std::exception& e)
            {
                mpl::error(log_category, "capture_impl() rollback failed: {}", e.what());
            }
        }
        else if (!head_preexisted) // nothing pre-existed; drop whatever we made
        {
            MP_FILEOPS.remove(head_path, ec);
            MP_FILEOPS.remove(snapshot_path, ec);
        }
    });

    if (head_preexisted)
    {
        MP_FILEOPS.rename(head_path, snapshot_path);
        head_became_snapshot = true;
    }
    else
    {
        const auto parent = get_parent();
        const auto target = parent ? make_snapshot_path(*parent) : base_vhdx_path;
        create_new_child_disk(target, snapshot_path);
    }

    // Create a new head from the snapshot
    create_new_child_disk(snapshot_path, head_path);

    rollback.dismiss();
}

void VirtDiskSnapshot::create_new_child_disk(const std::filesystem::path& parent,
                                             const std::filesystem::path& child) const
{
    mpl::debug(log_category, "create_new_child_disk() -> parent: {}, child: {}", parent, child);
    // The parent must already exist.
    if (!MP_FILEOPS.exists(parent))
        throw CreateVirtdiskSnapshotError{
            std::make_error_code(std::errc::no_such_file_or_directory),
            "Parent disk `{}` does not exist",
            parent};

    // The given child path must not exist
    if (MP_FILEOPS.exists(child))
        throw CreateVirtdiskSnapshotError{std::make_error_code(std::errc::file_exists),
                                          "Child disk `{}` already exists",
                                          child};

    const virtdisk::CreateVirtualDiskParameters params{
        .path = child,
        .predecessor = virtdisk::ParentPathParameters{parent}};

    if (const auto result = VirtDisk().create_virtual_disk(params); !result)
    {
        throw CreateVirtdiskSnapshotError{
            result,
            "Could not create the head differencing disk for the snapshot"};
    }

    mpl::debug(log_category, "Successfully created the child disk: `{}`", child);
}

std::vector<std::filesystem::path> VirtDiskSnapshot::get_disk_children(
    const std::filesystem::path& parent_disk) const
{
    const auto is_child_of_parent = [&](const std::filesystem::path& disk) {
        std::vector<std::filesystem::path> chain;
        return MP_FILEOPS.exists(disk) && VirtDisk().list_virtual_disk_chain(disk, chain, 2) &&
               chain.size() >= 2 &&
               MP_FILEOPS.weakly_canonical(chain[1]) == MP_FILEOPS.weakly_canonical(parent_disk);
    };

    std::vector<std::filesystem::path> result;

    // Snapshot files (all snapshots except this one) sitting directly on parent_disk.
    for (const auto& elem : vm.view_snapshots([this_index = get_index()](const Snapshot& ss) {
             return ss.get_index() != this_index;
         }))
    {
        auto path = make_snapshot_path(*elem);
        if (is_child_of_parent(path))
            result.push_back(std::move(path));
    }

    // The live head disk, if it sits directly on parent_disk. A missing or unreadable
    // head is simply treated as "not a child" rather than an error.
    if (auto head_path = make_head_disk_path(); is_child_of_parent(head_path))
        result.push_back(std::move(head_path));

    return result;
}

void VirtDiskSnapshot::erase_impl()
{
    const auto self_path = make_snapshot_path(*this);

    // Every differencing disk that sits directly on this snapshot: its snapshot children
    // plus the live head, if the head is attached here. Each must absorb this snapshot's
    // contents (via merge) so it stops depending on the snapshot we are deleting.
    auto children = get_disk_children(self_path);

    // This snapshot's grandchildren, each paired with the child it sits on. Rebuilding a
    // child in place gives its file a *new* VHDX identity, leaving every grandchild
    // recording its parent's *old* identity; the rebuild relinks them so the differencing
    // chain still validates when a grandchild is later opened with its parents (e.g. boot).
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> grandchildren;
    for (const auto& child_path : children)
        for (auto& grandchild : get_disk_children(child_path))
            grandchildren.emplace_back(std::move(grandchild), child_path);

    // Rebuild the direct children off a copy of self and relink the grandchildren onto
    // them, all transactionally: any failure restores the exact pre-erase state.
    ChildRebuild rebuild{self_path, std::move(children), std::move(grandchildren)};
    rebuild.begin(); // move self aside as the merge source; must precede the guard

    auto rollback = sg::make_scope_guard([&]() noexcept { rebuild.rollback(); });
    rebuild.stage();    // Pass 1: merge each child into a fresh copy of self (staged)
    rebuild.commit();   // Pass 2: swap each rebuilt child into place
    rebuild.reparent(); // refresh every grandchild's stored parent identity
    rollback.dismiss();
    rebuild.finalize(); // drop the now-unreferenced backups

    collapse_head_after_last_erase();
}

void VirtDiskSnapshot::collapse_head_after_last_erase()
{
    // If this was the last remaining snapshot, the head is now a plain differencing
    // disk sitting directly on the base. Collapse it back into the base so the VM
    // runs on a standalone disk again (restoring the pre-snapshot layout and, in
    // particular, re-enabling disk resize). Self is still counted here, so being the
    // last one means exactly one snapshot remains. Best-effort: the snapshot has
    // already been erased above, so a collapse failure must not fail the erase; it
    // just leaves the valid `base <- head` layout in place.
    if (vm.get_num_snapshots() != 1)
        return;

    try
    {
        collapse_head_into_base(base_vhdx_path);
    }
    catch (const std::exception& e)
    {
        mpl::warn(log_category,
                  "Could not collapse head into base after erasing the last snapshot: {}",
                  e.what());
    }
}

void VirtDiskSnapshot::collapse_head_into_base(const std::filesystem::path& base_vhdx_path)
{
    const auto head_path = base_vhdx_path.parent_path() / head_disk_name();

    // Nothing to do if there is no differencing head (the VM already runs on base).
    if (!MP_FILEOPS.exists(head_path))
        return;

    // Only collapse when the head is a *direct* child of the base. This holds exactly
    // when no snapshots remain; guarding on it means we never merge the head into the
    // wrong parent (e.g. a snapshot that other snapshots still depend on).
    std::vector<std::filesystem::path> chain{};
    if (!VirtDisk().list_virtual_disk_chain(head_path, chain, 2) || chain.size() != 2 ||
        MP_FILEOPS.weakly_canonical(chain[1]) != MP_FILEOPS.weakly_canonical(base_vhdx_path))
    {
        mpl::warn(log_category,
                  "Not collapsing head `{}`: it is not a direct child of base `{}`",
                  head_path,
                  base_vhdx_path);
        return;
    }

    // Merge the head down into the base. MergeVirtualDisk only *reads* the head and
    // *writes* into the base; it never touches the head file. That is what makes this
    // safe without copying the (potentially huge) base: if the merge fails partway,
    // the base may be partially written, but the head still holds every one of its
    // blocks and still overrides the base, so the `base <- head` chain keeps reading
    // correctly. We therefore only retire the head *after* a fully successful merge; on
    // failure (or a crash mid-merge) the VM simply keeps running on `base <- head` and
    // a later collapse/resize retries. This mirrors how Hyper-V merges checkpoints.
    if (const auto r = VirtDisk().merge_virtual_disk_into_parent(head_path); !r)
        throw CreateVirtdiskSnapshotError{r,
                                          "Could not merge head `{}` into base `{}`",
                                          head_path,
                                          base_vhdx_path};

    // The base now holds the merged-in live state. Retire the now-redundant head so
    // get_primary_disk_path() resolves to the standalone base. If this remove fails the
    // layout is still correct (`base <- head` reads identically to the merged base), so
    // it is only logged; a later collapse/resize will retry the removal.
    std::error_code ec{};
    MP_FILEOPS.remove(head_path, ec);
    if (ec)
        mpl::warn(log_category,
                  "Merged head `{}` into base `{}` but could not remove the redundant head: {}",
                  head_path,
                  base_vhdx_path,
                  ec.message());
}

void VirtDiskSnapshot::apply_impl()
{
    const auto head_path = make_head_disk_path();
    const auto snapshot_path = make_snapshot_path(*this);

    // Applying a snapshot discards the current head state. Build the replacement
    // head from the snapshot beside the live one first, so that if creation fails
    // the existing head stays intact (the VM remains bootable) instead of being
    // left with no head and silently falling back to the base disk. The temporary
    // name keeps the ".avhdx" extension because the VirtDisk API selects its
    // storage provider from the file extension.
    auto new_head_path = head_path;
    new_head_path.replace_extension(".new.avhdx"); // "head.avhdx" -> "head.new.avhdx"

    std::error_code ec{};
    MP_FILEOPS.remove(new_head_path, ec); // clear any leftover from a prior failed apply
    create_new_child_disk(snapshot_path, new_head_path); // throws here => old head untouched

    // Swap the freshly built head in for the old one. rename() replaces the
    // destination atomically, so there is never a moment with no head.
    MP_FILEOPS.rename(new_head_path, head_path);
    mpl::debug(log_category, "apply_impl() -> new head from {} -> {}", snapshot_path, head_path);
}

} // namespace multipass::hyperv::virtdisk
