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

#ifndef MULTIPASS_HYPERV_API_VIRTDISK_SNAPSHOT_H
#define MULTIPASS_HYPERV_API_VIRTDISK_SNAPSHOT_H

#include <hyperv_api/hyperv_api_wrapper_fwdecl.h>
#include <shared/base_snapshot.h>

#include <QString>
#include <filesystem>

namespace multipass::hyperv::virtdisk
{

class VirtDiskSnapshot : public BaseSnapshot
{
public:
    VirtDiskSnapshot(const std::string& name,
                     const std::string& comment,
                     const std::string& cloud_init_instance_id,
                     std::shared_ptr<Snapshot> parent,
                     const VMSpecs& specs,
                     const VirtualMachine& vm,
                     const VirtualMachineDescription& desc,
                     virtdisk_sptr_t virtdisk);
    VirtDiskSnapshot(const QString& filename,
                     VirtualMachine& vm,
                     const VirtualMachineDescription& desc,
                     virtdisk_sptr_t virtdisk);

    static std::string make_snapshot_name(const Snapshot& ss);
    std::filesystem::path make_snapshot_path(const Snapshot& ss) const;
    static constexpr std::string_view head_disk_name() noexcept
    {
        return "head.avhdx";
    }

protected:
    void capture_impl() override;
    void erase_impl() override;
    void apply_impl() override;

private:
    void create_new_child_disk(const std::filesystem::path& parent, const std::filesystem::path& child) const;
    void reparent_snapshot_disks(const VirtualMachine::SnapshotVista& snapshots,
                                 const std::filesystem::path& new_parent) const;
    const std::filesystem::path base_vhdx_path{};
    const VirtualMachine& vm;
    virtdisk_sptr_t virtdisk{nullptr};
};

} // namespace multipass::hyperv::virtdisk

#endif
