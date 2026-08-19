/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "migrating_hyperv_virtual_machine_factory.h"

#include "blocked_hyperv_virtual_machine.h"
#include "hyperv_disk_layout.h"
#include "hyperv_migration_state.h"
#include "hyperv_migrator.h"
#include "hyperv_virtual_machine.h"
#include "migrating_hyperv_virtual_machine.h"

#include <hyperv_api/hcs_virtual_machine.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#include <hyperv_api/virtdisk/virtdisk_create_virtual_disk_params.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/platform.h>
#include <multipass/file_ops.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>
#include <shared/windows/windows_feature_status.h>
#include <shared/windows/powershell.h>

#include <algorithm>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;

fs::path instance_path(const multipass::Path& path)
{
    return MP_PLATFORM.qstr_to_path(path);
}

bool has_native_hcs_state_files(const multipass::VirtualMachineDescription& description)
{
    auto guest_state = fs::path{description.image.image_path};
    guest_state.replace_extension(".vmgs");
    auto runtime_state = fs::path{description.image.image_path};
    runtime_state.replace_extension(".vmrs");
    return MP_FILEOPS.exists(guest_state) && MP_FILEOPS.exists(runtime_state);
}
} // namespace

multipass::hyperv::MigratingHyperVVirtualMachineFactory::
    MigratingHyperVVirtualMachineFactory(const Path& data_dir,
                                         AvailabilityZoneManager& az_manager)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir),
          az_manager),
      hcs_factory{data_dir, az_manager}
{
}

multipass::VirtualMachine::UPtr
multipass::hyperv::MigratingHyperVVirtualMachineFactory::create_virtual_machine(
    const VirtualMachineDescription& desc,
    const SSHKeyProvider& key_provider,
    VMStatusMonitor& monitor)
{
    const auto instance_dir = get_instance_directory(desc.vm_name);
    auto& zone = az_manager.get_zone(desc.zone);
    const auto blocked = [&](std::string reason) -> VirtualMachine::UPtr {
        return std::make_unique<BlockedHyperVVirtualMachine>(desc,
                                                             monitor,
                                                             key_provider,
                                                             zone,
                                                             instance_dir,
                                                             std::move(reason));
    };

    std::optional<HyperVMigrationState> migration_state;
    try
    {
        migration_state = HyperVMigrationState::load(instance_path(instance_dir));
    }
    catch (const std::exception& error)
    {
        return blocked(fmt::format("could not read migration metadata: {}", error.what()));
    }

    if (migration_state && migration_state->backend == HyperVBackend::hcs)
    {
        auto migrated_description = desc;
        migrated_description.image.image_path = migration_state->active_disk;
        return std::make_unique<HCSVirtualMachine>(default_hyperv_switch_guid,
                                                   migrated_description,
                                                   monitor,
                                                   key_provider,
                                                   zone,
                                                   instance_dir,
                                                   migration_state->hcs_state_file_stem);
    }

    bool legacy_exists;
    try
    {
        legacy_exists = HyperVDiskLayoutResolver::vm_exists(desc.vm_name);
    }
    catch (const std::exception& error)
    {
        return blocked(fmt::format("could not determine legacy Hyper-V ownership: {}",
                                   error.what()));
    }

    if (migration_state && !legacy_exists)
        return blocked("legacy migration metadata exists but the registered VM is missing");

    if (legacy_exists)
    {
        auto legacy = std::make_unique<HyperVVirtualMachine>(desc,
                                                             monitor,
                                                             key_provider,
                                                             zone,
                                                             instance_dir);
        auto migrator = std::make_unique<DefaultHyperVMigrator>(desc,
                                                                monitor,
                                                                key_provider,
                                                                zone,
                                                                instance_dir);
        return std::make_unique<MigratingHyperVVirtualMachine>(std::move(legacy),
                                                               std::move(migrator));
    }

    const auto pending_hcs = [&] {
        std::lock_guard lock{pending_instances_mutex};
        return pending_hcs_instances.erase(desc.vm_name) != 0;
    }();
    if (!pending_hcs && !has_native_hcs_state_files(desc))
        return blocked("backend ownership is ambiguous: no migration marker, legacy registration, "
                       "or native HCS state files were found");

    const HyperVMigrationState native_hcs_state{
        .backend = HyperVBackend::hcs,
        .active_disk = desc.image.image_path,
        .hcs_state_file_stem = desc.image.image_path,
    };
    native_hcs_state.persist(instance_path(instance_dir));

    return std::make_unique<HCSVirtualMachine>(default_hyperv_switch_guid,
                                               desc,
                                               monitor,
                                               key_provider,
                                               zone,
                                               instance_dir);
}

void multipass::hyperv::MigratingHyperVVirtualMachineFactory::prepare_networking(
    std::vector<NetworkInterface>& extra_interfaces)
{
    hcs_factory.prepare_networking(extra_interfaces);
}

multipass::VMImage
multipass::hyperv::MigratingHyperVVirtualMachineFactory::prepare_source_image(
    const VMImage& source_image)
{
    return hcs_factory.prepare_source_image(source_image);
}

void multipass::hyperv::MigratingHyperVVirtualMachineFactory::prepare_instance_image(
    const VMImage& instance_image,
    const VirtualMachineDescription& desc)
{
    hcs_factory.prepare_instance_image(instance_image, desc);
    std::lock_guard lock{pending_instances_mutex};
    pending_hcs_instances.insert(desc.vm_name);
}

void multipass::hyperv::MigratingHyperVVirtualMachineFactory::hypervisor_health_check()
{
    if (const auto hyperv_state = get_windows_feature_state(L"Microsoft-Hyper-V");
        hyperv_state && hyperv_state == WindowsFeatureState::Enabled)
        return;

    hcs_factory.hypervisor_health_check();
}

std::vector<multipass::NetworkInterfaceInfo>
multipass::hyperv::MigratingHyperVVirtualMachineFactory::networks() const
{
    return hcs_factory.networks();
}

std::string multipass::hyperv::MigratingHyperVVirtualMachineFactory::create_bridge_with(
    const NetworkInterfaceInfo& interface)
{
    return hcs_factory.create_bridge_for(interface);
}

void multipass::hyperv::MigratingHyperVVirtualMachineFactory::remove_resources_for_impl(
    const std::string& name)
{
    const auto state = HyperVMigrationState::load(instance_path(get_instance_directory(name)));
    if ((state && state->backend == HyperVBackend::hcs) ||
        !HyperVDiskLayoutResolver::vm_exists(name))
    {
        remove_hcs_resources(name);
        return;
    }

    PowerShell::exec({"Remove-VM", "-Name", QString::fromStdString(name), "-Force"}, name);
}

multipass::VirtualMachine::UPtr
multipass::hyperv::MigratingHyperVVirtualMachineFactory::clone_vm_impl(
    const std::string& source_vm_name,
    const VMSpecs&,
    const VirtualMachineDescription& desc,
    VMStatusMonitor& monitor,
    const SSHKeyProvider& key_provider)
{
    const auto source_state =
        HyperVMigrationState::load(instance_path(get_instance_directory(source_vm_name)));
    fs::path source_disk;

    if (source_state && source_state->backend == HyperVBackend::hcs)
        source_disk = source_state->active_disk;
    else if (HyperVDiskLayoutResolver::vm_exists(source_vm_name))
        source_disk = HyperVDiskLayoutResolver::active_disk(source_vm_name);
    else
    {
        const fs::path source_dir = instance_path(get_instance_directory(source_vm_name));
        for (const auto& entry : fs::directory_iterator{source_dir})
        {
            const auto extension = entry.path().extension();
            if (extension == ".vhdx" || extension == ".avhdx")
            {
                source_disk = entry.path();
                break;
            }
        }

        if (source_disk.empty())
            throw std::runtime_error{"Could not locate the source VM disk"};
    }

    const virtdisk::CreateVirtualDiskParameters params{
        .path = desc.image.image_path,
        .predecessor = virtdisk::SourcePathParameters{source_disk}};
    if (!virtdisk::VirtDisk().create_virtual_disk(params))
        throw std::runtime_error{"VHDX clone failed"};

    {
        std::lock_guard lock{pending_instances_mutex};
        pending_hcs_instances.insert(desc.vm_name);
    }
    return create_virtual_machine(desc, key_provider, monitor);
}
