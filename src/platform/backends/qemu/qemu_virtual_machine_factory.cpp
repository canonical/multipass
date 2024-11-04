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
#include <multipass/yaml_node_utils.h>
#include <shared/qemu_img_utils/qemu_img_utils.h>

#include <QRegularExpression>

namespace mp = multipass;
namespace mpl = multipass::logging;

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
    auto platform_ifs_info = MP_PLATFORM.get_network_interfaces_info();

    std::vector<NetworkInterfaceInfo> ret;
    for (const auto& ifs_info : platform_ifs_info)
    {
        const auto& info = ifs_info.second;
        const auto& type = info.type;

        if (qemu_platform->is_network_supported(type))
            ret.push_back(info);
    }

    qemu_platform->set_authorization(ret);

    return ret;
}

void mp::QemuVirtualMachineFactory::prepare_networking(std::vector<NetworkInterface>& extra_interfaces)
{
    if (qemu_platform->needs_network_prep())
        mp::BaseVirtualMachineFactory::prepare_networking(extra_interfaces);
}

std::string mp::QemuVirtualMachineFactory::create_bridge_with(const NetworkInterfaceInfo& interface)
{
    return qemu_platform->create_bridge_with(interface);
}

mp::VirtualMachine::UPtr mp::QemuVirtualMachineFactory::clone_vm_impl(const std::string& /*source_vm_name*/,
                                                                      const multipass::VMSpecs& /*src_vm_specs*/,
                                                                      const VirtualMachineDescription& desc,
                                                                      VMStatusMonitor& monitor,
                                                                      const SSHKeyProvider& key_provider)
{
    return std::make_unique<mp::QemuVirtualMachine>(desc,
                                                    qemu_platform.get(),
                                                    monitor,
                                                    key_provider,
                                                    get_instance_directory(desc.vm_name),
                                                    true);
}
