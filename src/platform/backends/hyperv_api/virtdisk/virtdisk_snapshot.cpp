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
      live_disk_path{desc.image.image_path},
      vm{vm}
{
}

VirtDiskSnapshot::VirtDiskSnapshot(const std::filesystem::path& filename,
                                   VirtualMachine& vm,
                                   const VirtualMachineDescription& desc)
    : BaseSnapshot(filename, vm, desc), live_disk_path{desc.image.image_path}, vm{vm}
{
}

std::string VirtDiskSnapshot::make_snapshot_filename(const Snapshot& ss)
{
    return fmt::format("{}.avhdx", ss.get_index());
}

std::filesystem::path VirtDiskSnapshot::make_snapshot_path(const Snapshot& ss) const
{
    return live_disk_path.parent_path() / make_snapshot_filename(ss);
}

void VirtDiskSnapshot::capture_impl()
{
    const auto snapshot_path = make_snapshot_path(*this);
    mpl::debug(vm.get_name(),
               "capture_impl() -> live_disk_path: {}, snapshot_path: {}",
               live_disk_path,
               snapshot_path);

    if (!MP_FILEOPS.exists(live_disk_path))
        throw CreateVirtdiskSnapshotError{
            std::make_error_code(std::errc::no_such_file_or_directory),
            "Live disk `{}` does not exist",
            live_disk_path};

    if (MP_FILEOPS.exists(snapshot_path))
        throw CreateVirtdiskSnapshotError{std::make_error_code(std::errc::file_exists),
                                          "Snapshot disk `{}` already exists",
                                          snapshot_path};

    bool live_disk_became_snapshot = false;
    auto rollback = sg::make_scope_guard([&]() noexcept {
        std::error_code ec{};
        if (live_disk_became_snapshot)
        {
            MP_FILEOPS.remove(live_disk_path, ec);
            try_rename(snapshot_path, live_disk_path);
        }
    });

    MP_FILEOPS.rename(live_disk_path, snapshot_path);
    live_disk_became_snapshot = true;
    create_new_child_disk(snapshot_path, live_disk_path);
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

    if (is_direct_child_of(live_disk_path, parent_disk))
        result.push_back(live_disk_path);

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
}

void VirtDiskSnapshot::apply_impl()
{
    const auto snapshot_path = make_snapshot_path(*this);

    if (!MP_FILEOPS.exists(live_disk_path))
        throw CreateVirtdiskSnapshotError{
            std::make_error_code(std::errc::no_such_file_or_directory),
            "Live disk `{}` does not exist",
            live_disk_path};

    auto new_live_disk_path = live_disk_path;
    new_live_disk_path.replace_extension(".new.vhdx");
    auto old_live_disk_path = live_disk_path;
    old_live_disk_path.replace_extension(".old.vhdx");

    MP_FILEOPS.remove(new_live_disk_path);
    MP_FILEOPS.remove(old_live_disk_path);
    create_new_child_disk(snapshot_path, new_live_disk_path);

    bool live_disk_moved = false;
    auto rollback = sg::make_scope_guard([&]() noexcept {
        std::error_code ec{};
        MP_FILEOPS.remove(new_live_disk_path, ec);
        if (live_disk_moved)
        {
            MP_FILEOPS.remove(live_disk_path, ec);
            try_rename(old_live_disk_path, live_disk_path);
        }
    });

    MP_FILEOPS.rename(live_disk_path, old_live_disk_path);
    live_disk_moved = true;
    MP_FILEOPS.rename(new_live_disk_path, live_disk_path);
    rollback.dismiss();

    std::error_code ec{};
    MP_FILEOPS.remove(old_live_disk_path, ec);
    if (ec)
        mpl::warn(vm.get_name(),
                  "Applied snapshot but could not remove old live disk `{}`: {}",
                  old_live_disk_path,
                  ec.message());

    mpl::debug(vm.get_name(),
               "apply_impl() -> new live disk from {} -> {}",
               snapshot_path,
               live_disk_path);
}

// Best-effort rename for noexcept rollback guards.
void VirtDiskSnapshot::try_rename(const std::filesystem::path& from,
                                  const std::filesystem::path& to) noexcept
{
    virtdisk::try_rename(vm.get_name(), from, to);
}

} // namespace multipass::hyperv::virtdisk
