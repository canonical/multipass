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

#include "qemu_virtual_machine_factory.h"
#include "qemu_virtual_machine.h"

#include <multipass/cloud_init_iso.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/process/simple_process_spec.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>
#include <multipass/yaml_node_utils.h>

#include <shared/qemu_img_utils/qemu_img_utils.h>

#include <QRegularExpression>
#include <scope_guard.hpp>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "qemu factory";
} // namespace

mp::QemuVirtualMachineFactory::QemuVirtualMachineFactory(const mp::Path& data_dir)
    : QemuVirtualMachineFactory{MP_QEMU_PLATFORM_FACTORY.make_qemu_platform(data_dir), data_dir}
{
}

mp::QemuVirtualMachineFactory::QemuVirtualMachineFactory(QemuPlatform::UPtr qemu_platform, const mp::Path& data_dir)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, qemu_platform->get_directory_name(), instances_subdir)),
      qemu_platform{std::move(qemu_platform)}
{
}

mp::VirtualMachine::UPtr mp::QemuVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                               const SSHKeyProvider& key_provider,
                                                                               VMStatusMonitor& monitor)
{
    return std::make_unique<mp::QemuVirtualMachine>(desc,
                                                    qemu_platform.get(),
                                                    monitor,
                                                    key_provider,
                                                    get_instance_directory(desc.vm_name));
}

mp::VirtualMachine::UPtr mp::QemuVirtualMachineFactory::create_vm_and_instance_disk_data(
    const QString& data_directory,
    const VMSpecs& src_vm_spec,
    const VMSpecs& dest_vm_spec,
    const std::string& source_name,
    const std::string& destination_name,
    const VMImage& dest_vm_image,
    const SSHKeyProvider& key_provider,
    VMStatusMonitor& monitor)
{
    const QString backend_data_direcotry =
        mp::utils::backend_directory_path(data_directory, get_backend_directory_name());
    const auto instances_data_directory =
        std::filesystem::path(backend_data_direcotry.toStdString()) / "vault" / "instances";
    const std::filesystem::path source_instance_data_directory = instances_data_directory / source_name;
    const std::filesystem::path dest_instance_data_directory = instances_data_directory / destination_name;

    // if any of the below code throw, then roll back and clean up the created instance folder
    auto rollback = sg::make_scope_guard([instance_directory_path = dest_instance_data_directory]() noexcept -> void {
        // use err_code to guarantee the two file operations below do not throw
        if (std::error_code err_code; MP_FILEOPS.exists(instance_directory_path, err_code))
        {
            MP_FILEOPS.remove(instance_directory_path, err_code);
        }
    });

    MP_FILEOPS.copy(source_instance_data_directory,
                    dest_instance_data_directory,
                    std::filesystem::copy_options::recursive);

    const fs::path cloud_init_config_iso_file_path = dest_instance_data_directory / "cloud-init-config.iso";
    CloudInitIso qemu_iso;
    qemu_iso.read_from(cloud_init_config_iso_file_path);

    std::string& meta_data_file_content = qemu_iso.at("meta-data");
    meta_data_file_content =
        mpu::emit_cloud_config(mpu::make_cloud_init_meta_config(destination_name, meta_data_file_content));

    if (qemu_iso.contains("network-config"))
    {
        std::string& network_config_file_content = qemu_iso.at("network-config");
        network_config_file_content =
            mpu::emit_cloud_config(mpu::make_cloud_init_network_config(dest_vm_spec.default_mac_address,
                                                                       dest_vm_spec.extra_interfaces,
                                                                       network_config_file_content));
    }

    qemu_iso.write_to(QString::fromStdString(cloud_init_config_iso_file_path.string()));

    // start to construct VirtualMachineDescription
    mp::VirtualMachineDescription dest_vm_desc{dest_vm_spec.num_cores,
                                               dest_vm_spec.mem_size,
                                               dest_vm_spec.disk_space,
                                               destination_name,
                                               dest_vm_spec.default_mac_address,
                                               dest_vm_spec.extra_interfaces,
                                               dest_vm_spec.ssh_username,
                                               dest_vm_image,
                                               cloud_init_config_iso_file_path.string().c_str(),
                                               {},
                                               {},
                                               {},
                                               {}};

    mp::VirtualMachine::UPtr cloned_instance = create_virtual_machine(dest_vm_desc, key_provider, monitor);
    cloned_instance->load_snapshots_and_update_unique_identifiers(src_vm_spec, dest_vm_spec, source_name);

    rollback.dismiss();
    return cloned_instance;
}

void mp::QemuVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
    qemu_platform->remove_resources_for(name);
}

mp::VMImage mp::QemuVirtualMachineFactory::prepare_source_image(const mp::VMImage& source_image)
{
    VMImage image{source_image};
    image.image_path = mp::backend::convert_to_qcow_if_necessary(source_image.image_path);
    mp::backend::amend_to_qcow2_v3(image.image_path);
    return image;
}

void mp::QemuVirtualMachineFactory::prepare_instance_image(const mp::VMImage& instance_image,
                                                           const VirtualMachineDescription& desc)
{
    mp::backend::resize_instance_image(desc.disk_space, instance_image.image_path);
}

void mp::QemuVirtualMachineFactory::hypervisor_health_check()
{
    qemu_platform->platform_health_check();
}

QString mp::QemuVirtualMachineFactory::get_backend_version_string() const
{
    auto process =
        mp::platform::make_process(simple_process_spec(QString("qemu-system-%1").arg(HOST_ARCH), {"--version"}));

    auto version_re = QRegularExpression("^QEMU emulator version ([\\d\\.]+)");
    auto exit_state = process->execute();

    if (exit_state.completed_successfully())
    {
        auto match = version_re.match(process->read_all_standard_output());

        if (match.hasMatch())
            return QString("qemu-%1").arg(match.captured(1));
        else
        {
            mpl::log(mpl::Level::error, category,
                     fmt::format("Failed to parse QEMU version out: '{}'", process->read_all_standard_output()));
            return QString("qemu-unknown");
        }
    }
    else
    {
        if (exit_state.error)
        {
            mpl::log(mpl::Level::error, category,
                     fmt::format("Qemu failed to start: {}", exit_state.failure_message()));
        }
        else if (exit_state.exit_code)
        {
            mpl::log(mpl::Level::error, category,
                     fmt::format("Qemu fail: '{}' with outputs:\n{}\n{}", exit_state.failure_message(),
                                 process->read_all_standard_output(), process->read_all_standard_error()));
        }
    }

    return QString("qemu-unknown");
}

QString mp::QemuVirtualMachineFactory::get_backend_directory_name() const
{
    return qemu_platform->get_directory_name();
}

auto mp::QemuVirtualMachineFactory::networks() const -> std::vector<NetworkInterfaceInfo>
{
    return qemu_platform->networks();
}

void mp::QemuVirtualMachineFactory::prepare_networking(std::vector<NetworkInterface>& extra_interfaces)
{
    return qemu_platform->prepare_networking(extra_interfaces);
}
