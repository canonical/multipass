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

static std::filesystem::path get_parent_disk_of(const std::filesystem::path& disk)
{
    std::vector<std::filesystem::path> chain{};
    constexpr auto depth = 2;
    const auto list_r = VirtDisk().list_virtual_disk_chain(disk, chain, depth);

    if (list_r && chain.size() == depth)
        return chain[1];

    throw CreateVirtdiskSnapshotError{list_r, "Could not determine parent disk of `{}`", disk};
}

// Best-effort rename for use inside noexcept rollback guards: MP_FILEOPS.rename has
// no error_code overload, so a failure during cleanup is caught and logged rather
// than allowed to escape (and mask) the original error.
static void try_rename(const std::filesystem::path& from,
                       const std::filesystem::path& to) noexcept
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

    const virtdisk::CreateVirtualDiskParameters params{.path = child,
                                                       .predecessor =
                                                           virtdisk::ParentPathParameters{parent}};

    if (const auto result = VirtDisk().create_virtual_disk(params); !result)
    {
        throw CreateVirtdiskSnapshotError{
            result,
            "Could not create the head differencing disk for the snapshot"};
    }

    mpl::debug(log_category, "Successfully created the child disk: `{}`", child);
}

VirtDiskSnapshot::SnapshotsMap VirtDiskSnapshot::get_children() const
{
    const auto self_path = make_snapshot_path(*this);
    const auto all_snapshots =
        vm.view_snapshots([this_index = this->get_index()](const Snapshot& ss) {
            // All except self
            return ss.get_index() != this_index;
        });

    SnapshotsMap result;

    for (const auto& elem : all_snapshots)
    {
        const auto& path = make_snapshot_path(*elem);
        std::vector<std::filesystem::path> chain;
        if (VirtDisk().list_virtual_disk_chain(path, chain, 2) && chain.size() >= 2)
        {
            if (MP_FILEOPS.weakly_canonical(chain[1]) == MP_FILEOPS.weakly_canonical(self_path))
                result.insert(std::make_pair(path, elem));
        }
    }
    return result;
}

bool VirtDiskSnapshot::is_head_attached_to_this() const
{
    const auto self_path = make_snapshot_path(*this);
    const auto head_path = make_head_disk_path();
    const auto head_parent_path = get_parent_disk_of(head_path);
    return MP_FILEOPS.weakly_canonical(head_parent_path) == MP_FILEOPS.weakly_canonical(self_path);
}

void VirtDiskSnapshot::erase_impl()
{
    const auto self_path = make_snapshot_path(*this);
    const auto head_path = make_head_disk_path();

    auto children = get_children();

    if (is_head_attached_to_this())
        children.insert({head_path, nullptr});

    // Move self aside as the pristine backup; self_path is recreated as merge
    // scratch for each child below.
    auto self_backup = self_path;
    self_backup += ".tmp";
    MP_FILEOPS.rename(self_path, self_backup);

    // {child, staged("<child>.new")} for every child we have processed.
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> staged;
    staged.reserve(children.size());

    // Restore the exact pre-erase state on any failure. Existence-driven so it is
    // correct no matter how far Pass 1/Pass 2 got, and noexcept so it never masks
    // the original error nor throws out of the guard.
    auto rollback = sg::make_scope_guard([&]() noexcept {
        std::error_code ec{};
        for (const auto& [child_path, staged_path] : staged)
        {
            auto backup_path = child_path;
            backup_path += ".old";
            if (MP_FILEOPS.exists(backup_path, ec)) // Pass 2 had started for this child
            {
                MP_FILEOPS.remove(child_path, ec);   // drop merged/partial content
                try_rename(backup_path, child_path); // restore the original
            }
            MP_FILEOPS.remove(staged_path, ec); // drop the staged .new if still present
        }
        MP_FILEOPS.remove(self_path, ec);   // drop leftover merge scratch
        try_rename(self_backup, self_path); // restore pristine self
    });

    // Pass 1: merge each child into a fresh copy of self and stash the result as
    // <child>.new. No original files are modified.
    for (const auto& [child_path, snapshot_ptr] : children)
    {
        auto staged_path = child_path;
        staged_path += ".new";

        MP_FILEOPS.copy(self_backup, self_path, {}); // fresh parent for this merge

        if (auto merge_r = VirtDisk().merge_virtual_disk_into_parent(child_path); !merge_r)
            throw CreateVirtdiskSnapshotError{merge_r,
                                              "Could not merge child disk `{}` into `{}`",
                                              child_path,
                                              self_path};

        MP_FILEOPS.rename(self_path, staged_path); // stash merged result
        staged.emplace_back(child_path, staged_path);
    }

    // Pass 2: commit. Preserve each original as <child>.old, then swap the merged
    // result in. Keeping the originals until the end lets the guard restore them if
    // a later swap fails.
    for (const auto& [child_path, staged_path] : staged)
    {
        auto backup_path = child_path;
        backup_path += ".old";
        MP_FILEOPS.rename(child_path, backup_path); // preserve original
        MP_FILEOPS.rename(staged_path, child_path); // put merged result in place
    }

    rollback.dismiss();

    // Success: drop the preserved originals and the self backup. Best-effort;
    // nothing references them anymore.
    std::error_code ec{};
    for (const auto& [child_path, staged_path] : staged)
    {
        auto backup_path = child_path;
        backup_path += ".old";
        MP_FILEOPS.remove(backup_path, ec);
    }
    MP_FILEOPS.remove(self_backup, ec);
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
