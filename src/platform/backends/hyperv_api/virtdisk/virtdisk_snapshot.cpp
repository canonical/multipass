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

#include <hyperv_api/virtdisk/virtdisk_child_rebuild.h>
#include <hyperv_api/virtdisk/virtdisk_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_utils.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/file_ops.h>
#include <multipass/logging/log.h>
#include <multipass/top_catch_all.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>
#include <scope_guard.hpp>

namespace
{
namespace mpl = multipass::logging;
namespace mhv = multipass::hyperv;
using PathPairs = std::vector<std::pair<std::filesystem::path, std::filesystem::path>>;

} // namespace

namespace multipass::hyperv::virtdisk
{

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
    mpl::debug(vm.get_name(),
               "capture_impl() -> head_path: {}, snapshot_path: {}",
               head_path,
               snapshot_path);

    const bool head_preexisted = MP_FILEOPS.exists(head_path);
    bool head_became_snapshot = false;

    // Leave no partial files after a failed capture.
    auto rollback = sg::make_scope_guard([&]() noexcept {
        std::error_code ec{};
        if (head_became_snapshot)
        {
            MP_FILEOPS.remove(head_path, ec);
            try_rename(snapshot_path, head_path);
        }
        else if (!head_preexisted)
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

    create_new_child_disk(snapshot_path, head_path);

    rollback.dismiss();
}

void VirtDiskSnapshot::create_new_child_disk(const std::filesystem::path& parent,
                                             const std::filesystem::path& child) const
{
    mpl::debug(vm.get_name(), "create_new_child_disk() -> parent: {}, child: {}", parent, child);
    if (!MP_FILEOPS.exists(parent))
        throw CreateVirtdiskSnapshotError{
            std::make_error_code(std::errc::no_such_file_or_directory),
            "Parent disk `{}` does not exist",
            parent};

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
            "Could not create child differencing disk `{}` from parent `{}`",
            child,
            parent};
    }

    mpl::debug(vm.get_name(), "Successfully created the child disk: `{}`", child);
}

std::vector<std::filesystem::path> VirtDiskSnapshot::get_children_of_disk(
    const std::filesystem::path& parent_disk) const
{
    std::vector<std::filesystem::path> result;

    const auto children_of_parent_disk = vm.view_snapshots(
        [this, &parent_disk](const Snapshot& ss) {
            const auto path = make_snapshot_path(ss);
            return &ss != this && is_direct_child_of(path, parent_disk);
        });

    std::ranges::transform(children_of_parent_disk,
                           std::back_inserter(result),
                           [this](const auto& ss) { return make_snapshot_path(*ss); });

    // Include the head disk if it's a direct child of the parent, too.
    if (auto head_path = make_head_disk_path(); is_direct_child_of(head_path, parent_disk))
        result.push_back(std::move(head_path));

    return result;
}

void VirtDiskSnapshot::erase_impl()
{
    const auto self_path = make_snapshot_path(*this);

    auto children = get_children_of_disk(self_path);

    PathPairs grandchildren;
    for (const auto& child_path : children)
        for (auto& grandchild : get_children_of_disk(child_path))
            grandchildren.emplace_back(std::move(grandchild), child_path);

    ChildRebuild rebuild{self_path, std::move(children), std::move(grandchildren), vm.get_name()};
    rebuild.begin();

    auto rollback = sg::make_scope_guard([&]() noexcept { rebuild.rollback(); });
    rebuild.stage();
    rebuild.commit();
    rebuild.reparent();
    rollback.dismiss();
    rebuild.finalize();

    collapse_head_after_last_erase();
}

void VirtDiskSnapshot::collapse_head_after_last_erase()
{
    // Self is still counted here, so the last snapshot means exactly one remains.
    if (vm.get_num_snapshots() != 1)
        return;

    collapse_head_into_base(base_vhdx_path);
}

void VirtDiskSnapshot::collapse_head_into_base(const std::string& vm_name,
                                               const std::filesystem::path& base_vhdx_path)
{
    const auto head_path = base_vhdx_path.parent_path() / head_disk_name();

    if (!MP_FILEOPS.exists(head_path))
        return;

    // Never merge the head into a snapshot that other snapshots still depend on.
    if (!is_direct_child_of(head_path, base_vhdx_path))
    {
        mpl::warn(vm_name,
                  "Not collapsing head `{}`: it is not a direct child of base `{}`",
                  head_path,
                  base_vhdx_path);
        return;
    }

    // Keep the head until merge succeeds so `base <- head` remains readable on failure.
    if (const auto r = VirtDisk().merge_virtual_disk_into_parent(head_path); !r)
        throw CreateVirtdiskSnapshotError{r,
                                          "Could not merge head `{}` into base `{}`",
                                          head_path,
                                          base_vhdx_path};

    // Removal failure is safe; a later collapse or resize will retry it.
    std::error_code ec{};
    MP_FILEOPS.remove(head_path, ec);
    if (ec)
        mpl::warn(vm_name,
                  "Merged head `{}` into base `{}` but could not remove the redundant head: {}",
                  head_path,
                  base_vhdx_path,
                  ec.message());
}

void VirtDiskSnapshot::collapse_head_into_base(const std::filesystem::path& base_vhdx_path)
{
    collapse_head_into_base(vm.get_name(), base_vhdx_path);
}

void VirtDiskSnapshot::apply_impl()
{
    const auto head_path = make_head_disk_path();
    const auto snapshot_path = make_snapshot_path(*this);

    // Build the replacement beside the live head so creation failure keeps the VM bootable.
    auto new_head_path = head_path;
    new_head_path.replace_extension(".new.avhdx"); // "head.avhdx" -> "head.new.avhdx"

    MP_FILEOPS.remove(new_head_path);
    create_new_child_disk(snapshot_path, new_head_path);

    MP_FILEOPS.rename(new_head_path, head_path);
    mpl::debug(vm.get_name(), "apply_impl() -> new head from {} -> {}", snapshot_path, head_path);
}

// Best-effort rename for noexcept rollback guards.
void VirtDiskSnapshot::try_rename(const std::filesystem::path& from,
                                  const std::filesystem::path& to) noexcept
{
    virtdisk::try_rename(vm.get_name(), from, to);
}

} // namespace multipass::hyperv::virtdisk
