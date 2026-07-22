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

// Best-effort rename for noexcept rollback guards.
static void try_rename(const std::filesystem::path& from, const std::filesystem::path& to) noexcept
{
    try
    {
        MP_FILEOPS.rename(from, to);
    }
    catch (const std::exception& e)
    {
        mpl::error(log_category,
                   "Failed to rename `{}` -> `{}` during rollback: {}",
                   from,
                   to,
                   e.what());
    }
}

namespace
{
// Keep backup/scratch files beside their disk.
std::filesystem::path with_suffix(std::filesystem::path p, const char* suffix)
{
    p += suffix;
    return p;
}

bool is_direct_child_of(const std::filesystem::path& disk,
                        const std::filesystem::path& parent_disk,
                        bool required)
{
    if (!MP_FILEOPS.exists(disk))
        return false;

    std::vector<std::filesystem::path> chain;
    if (const auto r = VirtDisk().list_virtual_disk_chain(disk, chain, 2); !r)
    {
        if (required)
            throw CreateVirtdiskSnapshotError{r,
                                              "Could not inspect virtual disk chain for `{}`",
                                              disk};

        mpl::warn(log_category, "Could not inspect optional disk chain for `{}`: {}", disk, r);
        return false;
    }

    return chain.size() >= 2 &&
           MP_FILEOPS.weakly_canonical(chain[1]) == MP_FILEOPS.weakly_canonical(parent_disk);
}

/**
 * Rebuild a deleted snapshot's direct children transactionally.
 *
 * Each child is rebuilt from the deleted snapshot. Rebuilt disks get new VHDX
 * identities, so grandchildren must be reparented to the rebuilt files.
 *
 * Rollback restores the original files and parent links.
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

    // Move self aside before arming rollback.
    void begin()
    {
        MP_FILEOPS.rename(self_path, self_backup);
    }

    // Merge each child into a fresh copy of self, staged as "<child>.new".
    void stage()
    {
        for (const auto& child_path : children)
        {
            auto staged_path = with_suffix(child_path, ".new");

            MP_FILEOPS.copy(self_backup, self_path, {});

            if (auto merge_r = VirtDisk().merge_virtual_disk_into_parent(child_path); !merge_r)
                throw CreateVirtdiskSnapshotError{merge_r,
                                                  "Could not merge child disk `{}` into `{}`",
                                                  child_path,
                                                  self_path};

            MP_FILEOPS.rename(self_path, staged_path);
            staged.emplace_back(child_path, staged_path);
        }
    }

    // Swap staged children into place, keeping originals for rollback.
    void commit()
    {
        for (const auto& [child_path, staged_path] : staged)
        {
            MP_FILEOPS.rename(child_path, with_suffix(child_path, ".old"));
            MP_FILEOPS.rename(staged_path, child_path);
        }
    }

    // Refresh grandchildren after their rebuilt parents get new VHDX identities.
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

    // Drop unreferenced backups.
    void finalize() noexcept
    {
        std::error_code ec{};
        for (const auto& [child_path, staged_path] : staged)
            MP_FILEOPS.remove(with_suffix(child_path, ".old"), ec);
        MP_FILEOPS.remove(self_backup, ec);
    }

    // Restore the pre-erase state without masking the original error.
    void rollback() noexcept
    {
        std::error_code ec{};
        for (const auto& [child_path, staged_path] : staged)
        {
            const auto backup_path = with_suffix(child_path, ".old");
            if (MP_FILEOPS.exists(backup_path, ec))
            {
                MP_FILEOPS.remove(child_path, ec);
                try_rename(backup_path, child_path);
            }
            MP_FILEOPS.remove(staged_path, ec);
        }
        MP_FILEOPS.remove(self_path, ec);
        try_rename(self_backup, self_path);

        // Restore already-updated parent links.
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

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> grandchildren;

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> staged;

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

    // Leave no partial files after a failed capture.
    auto rollback = sg::make_scope_guard([&]() noexcept {
        std::error_code ec{};
        if (head_became_snapshot)
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
    mpl::debug(log_category, "create_new_child_disk() -> parent: {}, child: {}", parent, child);
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

    mpl::debug(log_category, "Successfully created the child disk: `{}`", child);
}

std::vector<std::filesystem::path> VirtDiskSnapshot::get_disk_children(
    const std::filesystem::path& parent_disk) const
{
    std::vector<std::filesystem::path> result;

    for (const auto& elem : vm.view_snapshots([this_index = get_index()](const Snapshot& ss) {
             return ss.get_index() != this_index;
         }))
    {
        auto path = make_snapshot_path(*elem);
        if (is_direct_child_of(path, parent_disk, true))
            result.push_back(std::move(path));
    }

    if (auto head_path = make_head_disk_path(); is_direct_child_of(head_path, parent_disk, true))
        result.push_back(std::move(head_path));

    return result;
}

void VirtDiskSnapshot::erase_impl()
{
    const auto self_path = make_snapshot_path(*this);

    auto children = get_disk_children(self_path);

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> grandchildren;
    for (const auto& child_path : children)
        for (auto& grandchild : get_disk_children(child_path))
            grandchildren.emplace_back(std::move(grandchild), child_path);

    ChildRebuild rebuild{self_path, std::move(children), std::move(grandchildren)};
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

    if (!MP_FILEOPS.exists(head_path))
        return;

    // Never merge the head into a snapshot that other snapshots still depend on.
    if (!is_direct_child_of(head_path, base_vhdx_path, false))
    {
        mpl::warn(log_category,
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

    // Build the replacement beside the live head so creation failure keeps the VM bootable.
    auto new_head_path = head_path;
    new_head_path.replace_extension(".new.avhdx"); // "head.avhdx" -> "head.new.avhdx"

    std::error_code ec{};
    MP_FILEOPS.remove(new_head_path, ec);
    create_new_child_disk(snapshot_path, new_head_path);

    MP_FILEOPS.rename(new_head_path, head_path);
    mpl::debug(log_category, "apply_impl() -> new head from {} -> {}", snapshot_path, head_path);
}

} // namespace multipass::hyperv::virtdisk
