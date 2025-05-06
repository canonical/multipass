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

#include <hyperv_api/virtdisk/virtdisk_wrapper_interface.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

namespace
{
constexpr auto kLogCategory = "virtdisk-snapshot";
}

namespace multipass::hyperv::virtdisk
{

struct CreateVirtdiskSnapshotError : public FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

VirtDiskSnapshot::VirtDiskSnapshot(const std::string& name,
                                   const std::string& comment,
                                   const std::string& instance_id,
                                   std::shared_ptr<Snapshot> parent,
                                   const VMSpecs& specs,
                                   const VirtualMachine& vm,
                                   const VirtualMachineDescription& desc,
                                   virtdisk_sptr_t virtdisk)
    : BaseSnapshot(name, comment, instance_id, std::move(parent), specs, vm),
      base_vhdx_path{desc.image.image_path.toStdString()},
      vm{vm},
      virtdisk{virtdisk}
{
}

VirtDiskSnapshot::VirtDiskSnapshot(const QString& filename,
                                   VirtualMachine& vm,
                                   const VirtualMachineDescription& desc,
                                   virtdisk_sptr_t virtdisk)
    : BaseSnapshot(filename, vm, desc), base_vhdx_path{desc.image.image_path.toStdString()}, vm{vm}, virtdisk{virtdisk}
{
}

std::string VirtDiskSnapshot::make_snapshot_name(const Snapshot& ss)
{
    constexpr static auto kSnapshotNameFormat = "{}.avhdx";
    return fmt::format(kSnapshotNameFormat, ss.get_name());
}

std::filesystem::path VirtDiskSnapshot::make_snapshot_path(const Snapshot& ss) const
{
    return base_vhdx_path.parent_path() / make_snapshot_name(ss);
}

void VirtDiskSnapshot::capture_impl()
{
    assert(virtdisk);

    const auto& head_path = base_vhdx_path.parent_path() / head_disk_name();
    const auto& snapshot_path = make_snapshot_path(*this);

    if (!std::filesystem::exists(head_path))
    {
        const auto& parent = get_parent();
        create_new_child_disk(parent ? make_snapshot_path(*parent) : base_vhdx_path, head_path);
    }

    // Step 1: Rename current head to snapshot name
    std::filesystem::rename(head_path, snapshot_path);

    // Step 2: Create a new head from the snapshot
    create_new_child_disk(snapshot_path, head_path);
}

void VirtDiskSnapshot::create_new_child_disk(const std::filesystem::path& parent,
                                             const std::filesystem::path& child) const
{
    if (!std::filesystem::exists(parent))
    {
        throw CreateVirtdiskSnapshotError("The parent disk {} does not exist!", child);
    }

    if (std::filesystem::exists(child))
    {
        throw CreateVirtdiskSnapshotError("The target child disk {} already exist!", child);
    }

    virtdisk::CreateVirtualDiskParameters params{};
    params.path = child;
    params.predecessor = virtdisk::ParentPathParameters{parent};

    if (const auto r = virtdisk->create_virtual_disk(params); !r)
        throw CreateVirtdiskSnapshotError{
            "Could not create the head differencing disk for the snapshot. Error code: {}",
            r};
}

void VirtDiskSnapshot::reparent_snapshot_disks(const VirtualMachine::SnapshotVista& snapshots,
                                               const std::filesystem::path& new_parent) const
{
    for (const auto& child : snapshots)
    {
        const auto& child_path = make_snapshot_path(*child);
        if (const auto result = virtdisk->reparent_virtual_disk(child_path, new_parent); !result)
        {
            mpl::warn(kLogCategory, "Could not reparent `{}` to `{}`. Error code: {}", child_path, new_parent, result);
            continue;
        }
        mpl::debug(kLogCategory, "Successfully reparented the child disk `{}` to `{}`", child_path, new_parent);
    }
}

void VirtDiskSnapshot::erase_impl()
{
    assert(virtdisk);
    const auto& parent = get_parent();
    const auto& self_path = make_snapshot_path(*this);

    //  1: Merge this to its parent
    if (const auto merge_r = virtdisk->merge_virtual_disk_to_parent(self_path); merge_r)
    {
        const auto& parent_path = parent ? make_snapshot_path(*parent) : base_vhdx_path;
        mpl::debug(kLogCategory, "Successfully merged differencing disk `{}` to its parent", self_path, parent_path);

        // The actual reparenting of the children needs to happen here.
        // Reparenting is not a simple "-> now this is your parent" like thing. The children
        // include parent's metadata calculated based on actual contents, and merging a child disk
        // to parent updates its parent's metadata, too.
        // Hence, the reparenting operation not only needs to happen to the orphaned children,
        // but also to the existing children of the parent as well, so the updated metadata of the
        // parent could be reflected to the all.
        const auto children_to_reparent =
            vm.view_snapshots([&parent, this_index = this->get_index()](const Snapshot& ss) {
                return
                    // Exclude self.
                    (ss.get_index() != this_index) &&
                    // set_parent() for the orphaned children happens before erase() call
                    // so they're already adopted by the self's parent at this point.
                    (ss.get_parents_index() == parent->get_index());
            });
        reparent_snapshot_disks(children_to_reparent, parent_path);
    }
    else
    {
        throw CreateVirtdiskSnapshotError{"Could not merge differencing disk to parent. Error code: {}", merge_r};
    }
    // Finally, erase the merged disk.
    mpl::debug(kLogCategory, "Removing snapshot file: `{}`", self_path);
    std::filesystem::remove(self_path);
}

void VirtDiskSnapshot::apply_impl()
{
    assert(virtdisk);

    const auto& head_path = base_vhdx_path.parent_path() / head_disk_name();
    const auto& snapshot_path = make_snapshot_path(*this);

    // Restoring a snapshot means we're discarding the head state.
    std::error_code ec{};
    std::filesystem::remove(head_path, ec);
    mpl::debug(kLogCategory, "apply_impl() -> {} remove {}", head_path, ec.message());

    // Create a new head from the snapshot
    create_new_child_disk(snapshot_path, head_path);
}

} // namespace multipass::hyperv::virtdisk
