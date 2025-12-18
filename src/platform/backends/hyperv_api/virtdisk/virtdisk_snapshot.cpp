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
}

namespace multipass::hyperv::virtdisk
{

struct CreateVirtdiskSnapshotError : FormattedExceptionBase<std::system_error>
{
    using FormattedExceptionBase::FormattedExceptionBase;
};

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
    constexpr auto snapshot_name_format = "{}.avhdx";
    return fmt::format(snapshot_name_format, ss.get_name());
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
    const auto& head_path = make_head_disk_path();
    const auto& snapshot_path = make_snapshot_path(*this);
    mpl::debug(log_category,
               "capture_impl() -> head_path: {}, snapshot_path: {}",
               head_path,
               snapshot_path);

    // Check if head disk already exists. The head disk may not exist for a VM
    // that has no snapshots yet.
    if (!MP_FILEOPS.exists(head_path))
    {
        const auto& parent = get_parent();
        const auto& target = parent ? make_snapshot_path(*parent) : base_vhdx_path;
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

    virtdisk::CreateVirtualDiskParameters params{};
    params.predecessor = virtdisk::ParentPathParameters{parent};
    params.path = child;
    const auto result = VirtDisk().create_virtual_disk(params);
    if (result)
    {
        mpl::debug(log_category, "Successfully created the child disk: `{}`", child);
        return;
    }

    throw CreateVirtdiskSnapshotError{
        result,
        "Could not create the head differencing disk for the snapshot"};
}

void VirtDiskSnapshot::reparent_snapshot_disks(const VirtualMachine::SnapshotVista& snapshots,
                                               const std::filesystem::path& new_parent) const
{
    mpl::debug(log_category,
               "reparent_snapshot_disks() -> snapshots_count: {}, new_parent: {}",
               snapshots.size(),
               new_parent);

    // The parent must already exist.
    if (!MP_FILEOPS.exists(new_parent))
        throw CreateVirtdiskSnapshotError{
            std::make_error_code(std::errc::no_such_file_or_directory),
            "Parent disk `{}` does not exist",
            new_parent};

    for (const auto& child : snapshots)
    {
        const auto& child_path = make_snapshot_path(*child);

        if (!MP_FILEOPS.exists(child_path))
            throw CreateVirtdiskSnapshotError{std::make_error_code(std::errc::file_exists),
                                              "Child disk `{}` does not exists",
                                              child_path};
        if (const auto result = VirtDisk().reparent_virtual_disk(child_path, new_parent); !result)
        {
            mpl::warn(log_category,
                      "Could not reparent `{}` to `{}`: {}",
                      child_path,
                      new_parent,
                      static_cast<std::error_code>(result));
            continue;
        }
        mpl::debug(log_category,
                   "Successfully reparented the child disk `{}` to `{}`",
                   child_path,
                   new_parent);
    }
}

void VirtDiskSnapshot::erase_impl()
{

    const auto& parent = get_parent();
    const auto& self_path = make_snapshot_path(*this);
    const auto& head_path = make_head_disk_path();
    mpl::debug(log_category,
               "erase_impl() -> parent: {}, self_path: {}",
               parent ? parent->get_name() : "<none>",
               self_path);

    const auto& head_parent = [&]() {
        std::vector<std::filesystem::path> chain{};
        constexpr auto depth = 2;
        const auto list_r = VirtDisk().list_virtual_disk_chain(head_path, chain, depth);

        if (chain.size() == depth)
            return chain[1];

        throw CreateVirtdiskSnapshotError{list_r, "Could not determine head disk's parent"};
    }();

    const bool should_merge_head = (self_path == head_parent) && vm.get_num_snapshots() == 1;
    const bool should_reparent_head = (self_path == head_parent) && vm.get_num_snapshots() > 1;

    // If the head's parent is this, and this is the last snapshot,
    // we need to merge the head to the snapshot first.
    if (should_merge_head)
    {
        // Head is attached to the snapshot that's going to be deleted.
        if (const auto merge_r = VirtDisk().merge_virtual_disk_to_parent(head_path); merge_r)
        {
            mpl::debug(log_category,
                       "Successfully merged head differencing disk `{}` to parent disk `{}`",
                       head_path,
                       self_path);
            mpl::debug(log_category, "Removing the merged head disk file: `{}`", head_path);
            MP_FILEOPS.remove(head_path);
        }
        else
        {
            throw CreateVirtdiskSnapshotError{
                merge_r,
                "Could not merge head differencing disk to the edge snapshot"};
        }
    }

    //  1: Merge this to its parent
    if (const auto merge_r = VirtDisk().merge_virtual_disk_to_parent(self_path); merge_r)
    {
        const auto& parent_path = parent ? make_snapshot_path(*parent) : base_vhdx_path;
        mpl::debug(log_category,
                   "Successfully merged differencing disk `{}` to parent disk `{}`",
                   self_path,
                   parent_path);

        // The actual reparenting of the children needs to happen here. Reparenting is not a simple
        // "-> now this is your parent" like thing. The children include parent's metadata
        // calculated based on actual contents, and merging a child disk to parent updates its
        // parent's metadata, too. Hence, the reparenting operation not only needs to happen to the
        // orphaned children, but also to the existing children of the parent as well, so the
        // updated metadata of the parent could be reflected to the all.
        const auto children_to_reparent =
            vm.view_snapshots([&parent, this_index = this->get_index()](const Snapshot& ss) {
                return
                    // Exclude self.
                    (ss.get_index() != this_index) &&
                    // set_parent() for the orphaned children happens before erase() call
                    // so they're already adopted by the self's parent at this point.
                    (parent ? (ss.get_parents_index() == parent->get_index())
                            : ss.get_parents_index() == 0);
            });
        reparent_snapshot_disks(children_to_reparent, parent_path);

        if (should_reparent_head)
        {
            if (const auto res = VirtDisk().reparent_virtual_disk(head_path, parent_path); res)
            {
                mpl::debug(log_category, "Reparented head {} to {}", head_path, parent_path);
            }
            else
            {
                throw CreateVirtdiskSnapshotError{
                    res,
                    "Could not reparent head differencing disk to the parent"};
            }
        }
    }
    else
    {
        throw CreateVirtdiskSnapshotError{merge_r, "Could not merge differencing disk to parent"};
    }
    // Finally, erase the merged disk.
    mpl::debug(log_category, "Removing snapshot file: `{}`", self_path);
    MP_FILEOPS.remove(self_path);
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
