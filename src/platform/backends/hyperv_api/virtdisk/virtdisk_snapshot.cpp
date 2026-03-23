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
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

namespace
{
constexpr auto log_category = "virtdisk-snapshot";
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

    if (chain.size() == depth)
        return chain[1];

    throw CreateVirtdiskSnapshotError{list_r, "Could not determine parent disk of `{}`", disk};
}

VirtDiskSnapshot::VirtDiskSnapshot(const std::string& name,
                                   const std::string& comment,
                                   const std::string& instance_id,
                                   std::shared_ptr<Snapshot> parent,
                                   const VMSpecs& specs,
                                   const VirtualMachine& vm,
                                   const VirtualMachineDescription& desc)
    : BaseSnapshot(name, comment, instance_id, std::move(parent), specs, vm),
      base_vhdx_path{desc.image.image_path.toStdString()},
      vm{vm}
{
}

VirtDiskSnapshot::VirtDiskSnapshot(const QString& filename,
                                   VirtualMachine& vm,
                                   const VirtualMachineDescription& desc)
    : BaseSnapshot(filename, vm, desc), base_vhdx_path{desc.image.image_path.toStdString()}, vm{vm}
{
}

std::string VirtDiskSnapshot::make_snapshot_filename(const Snapshot& ss)
{
    return fmt::format("{}.avhdx", ss.get_name());
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

    // Check if head disk already exists. The head disk may not exist for a VM
    // that has no snapshots yet.
    if (!MP_FILEOPS.exists(head_path))
    {
        const auto parent = get_parent();
        const auto target = parent ? make_snapshot_path(*parent) : base_vhdx_path;
        create_new_child_disk(target, head_path);
    }

    // Step 1: Rename current head to snapshot name
    MP_FILEOPS.rename(head_path, snapshot_path);

    // Step 2: Create a new head from the snapshot
    create_new_child_disk(snapshot_path, head_path);
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

    auto self_temp_path = self_path;
    self_temp_path += ".tmp";
    MP_FILEOPS.rename(self_path, self_temp_path);

    // Pass 1: merge each child into a copy, stash the result as child.new
    // No original files are modified. If anything fails, we clean up and bail.
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
        staged; // {child, child.new}

    auto rollback = [&] {
        for (const auto& [original, stashed] : staged)
        {
            std::error_code ec;
            MP_FILEOPS.remove(stashed, ec);
        }
        // Restore the snapshot file
        MP_FILEOPS.remove(self_path);
        MP_FILEOPS.rename(self_temp_path, self_path);
    };

    for (const auto& [child_path, snapshot_ptr] : children)
    {
        MP_FILEOPS.copy(self_temp_path, self_path, {});

        if (auto merge_r = VirtDisk().merge_virtual_disk_into_parent(child_path); !merge_r)
        {
            rollback();
            throw CreateVirtdiskSnapshotError{merge_r,
                                              "Could not merge child disk `{}` into `{}`",
                                              child_path,
                                              self_path};
        }

        // Stash merged result. Original child_path is untouched.
        auto stash_path = child_path;
        stash_path += ".new";
        MP_FILEOPS.rename(self_path, stash_path);
        staged.emplace_back(child_path, stash_path);
    }

    // Pass 2: all merges succeeded. Commit by swapping originals with results.
    // This is a series of renames, which are atomic per-file on NTFS.
    for (const auto& [child_path, stash_path] : staged)
    {
        MP_FILEOPS.remove(child_path);
        MP_FILEOPS.rename(stash_path, child_path);
    }

    MP_FILEOPS.remove(self_temp_path);
}

void VirtDiskSnapshot::apply_impl()
{
    const auto& head_path = base_vhdx_path.parent_path() / head_disk_name();
    const auto& snapshot_path = make_snapshot_path(*this);

    // Restoring a snapshot means we're discarding the head state.
    std::error_code ec{};
    MP_FILEOPS.remove(head_path, ec);
    mpl::debug(log_category, "apply_impl() -> {} remove: {}", head_path, ec);

    // Create a new head from the snapshot
    create_new_child_disk(snapshot_path, head_path);
}

} // namespace multipass::hyperv::virtdisk
