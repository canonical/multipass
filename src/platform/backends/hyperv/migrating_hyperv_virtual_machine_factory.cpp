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

#include "blocked_virtual_machine.h"
#include "hcs_ownership.h"
#include "hyperv_disk_layout.h"
#include "hyperv_migrator.h"
#include "hyperv_virtual_machine.h"
#include "migrating_hyperv_virtual_machine.h"

#include <hyperv_api/hcs_virtual_machine.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#include <hyperv_api/virtdisk/virtdisk_create_virtual_disk_params.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/file_ops.h>
#include <multipass/platform.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>
#include <shared/windows/powershell.h>
#include <shared/windows/windows_feature_status.h>

#include <fmt/format.h>

namespace
{
namespace fs = std::filesystem;
namespace mp = multipass;
namespace mhv = multipass::hyperv;

fs::path instance_path(const multipass::Path& path)
{
    return MP_PLATFORM.qstr_to_path(path);
}

fs::path with_extension(fs::path path, const char* extension)
{
    return path.replace_extension(extension);
}

bool has_hcs_state_files(const fs::path& stem)
{
    return MP_FILEOPS.exists(with_extension(stem, ".vmgs")) &&
           MP_FILEOPS.exists(with_extension(stem, ".vmrs"));
}

mhv::HCSOwnership persist_hcs_ownership(const fs::path& disk,
                                        const fs::path& state_file_stem,
                                        const multipass::Path& instance_dir)
{
    const mhv::HCSOwnership ownership{
        .active_disk = disk,
        .state_file_stem = state_file_stem,
    };
    ownership.persist(instance_path(instance_dir));
    return ownership;
}

fs::path source_disk_for(const std::string& name, const mp::Path& instance_dir)
{
    if (const auto ownership = mhv::HCSOwnership::load(instance_path(instance_dir)))
    {
        if (mhv::legacy_vm_exists(name))
            throw std::runtime_error{
                "HCS ownership marker and legacy Hyper-V registration both exist"};
        return ownership->active_disk;
    }

    if (mhv::legacy_vm_exists(name))
        return mhv::legacy_active_disk(name);

    throw std::runtime_error{"Could not determine source VM disk ownership"};
}

} // namespace

multipass::hyperv::MigratingHyperVVirtualMachineFactory::MigratingHyperVVirtualMachineFactory(
    const Path& data_dir,
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
    const auto make_blocked = [&](std::string reason) -> VirtualMachine::UPtr {
        return std::make_unique<BlockedVirtualMachine>(desc,
                                                       monitor,
                                                       key_provider,
                                                       zone,
                                                       instance_dir,
                                                       std::move(reason));
    };
    const auto make_hcs = [&](const HCSOwnership& ownership) -> VirtualMachine::UPtr {
        auto hcs_description = desc;
        hcs_description.image.image_path = ownership.active_disk;
        return std::make_unique<HCSVirtualMachine>(default_hyperv_switch_guid,
                                                   hcs_description,
                                                   monitor,
                                                   key_provider,
                                                   zone,
                                                   instance_dir,
                                                   ownership.state_file_stem);
    };
    const auto make_migrating = [&]() -> VirtualMachine::UPtr {
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
    };

    try
    {
        if (const auto ownership = HCSOwnership::load(instance_path(instance_dir)))
        {
            if (legacy_vm_exists(desc.vm_name))
                throw std::runtime_error{
                    "HCS ownership marker and legacy Hyper-V registration both exist"};
            if (!MP_FILEOPS.exists(ownership->active_disk))
                throw std::runtime_error{
                    fmt::format("HCS active disk '{}' does not exist", ownership->active_disk)};
            return make_hcs(*ownership);
        }

        if (legacy_vm_exists(desc.vm_name))
            return make_migrating();

        if (has_hcs_state_files(desc.image.image_path))
            return make_hcs(
                persist_hcs_ownership(desc.image.image_path, desc.image.image_path, instance_dir));

        return make_blocked("backend ownership is ambiguous: no HCS marker, legacy registration, "
                            "or native HCS state files were found");
    }
    catch (const std::exception& error)
    {
        return make_blocked(fmt::format("could not determine Hyper-V ownership: {}", error.what()));
    }
}

void multipass::hyperv::MigratingHyperVVirtualMachineFactory::prepare_networking(
    std::vector<NetworkInterface>& extra_interfaces)
{
    hcs_factory.prepare_networking(extra_interfaces);
}

multipass::VMImage multipass::hyperv::MigratingHyperVVirtualMachineFactory::prepare_source_image(
    const VMImage& source_image)
{
    return hcs_factory.prepare_source_image(source_image);
}

void multipass::hyperv::MigratingHyperVVirtualMachineFactory::prepare_instance_image(
    const VMImage& instance_image,
    const VirtualMachineDescription& desc)
{
    hcs_factory.prepare_instance_image(instance_image, desc);
    persist_hcs_ownership(instance_image.image_path,
                          instance_image.image_path,
                          get_instance_directory(desc.vm_name));
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
    if (HCSOwnership::load(instance_path(get_instance_directory(name))))
    {
        remove_hcs_resources(name);
        return;
    }

    if (legacy_vm_exists(name))
    {
        PowerShell::exec({"Remove-VM", "-Name", QString::fromStdString(name), "-Force"}, name);
        return;
    }

    remove_hcs_resources(name);
}

multipass::VirtualMachine::UPtr
multipass::hyperv::MigratingHyperVVirtualMachineFactory::clone_vm_impl(
    const std::string& source_vm_name,
    const VMSpecs&,
    const VirtualMachineDescription& desc,
    VMStatusMonitor& monitor,
    const SSHKeyProvider& key_provider)
{
    const virtdisk::CreateVirtualDiskParameters params{
        .path = desc.image.image_path,
        .predecessor = virtdisk::SourcePathParameters{
            source_disk_for(source_vm_name, get_instance_directory(source_vm_name))}};
    if (!virtdisk::VirtDisk().create_virtual_disk(params))
        throw std::runtime_error{"VHDX clone failed"};

    persist_hcs_ownership(desc.image.image_path,
                          desc.image.image_path,
                          get_instance_directory(desc.vm_name));
    return create_virtual_machine(desc, key_provider, monitor);
}
