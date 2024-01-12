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
#ifndef MULTIPASS_QEMU_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_QEMU_VIRTUAL_MACHINE_FACTORY_H

#include "qemu_platform.h"

#include <multipass/path.h>
#include <shared/base_virtual_machine_factory.h>

#include <QString>

#include <string>

namespace multipass
{
class QemuVirtualMachineFactory final : public BaseVirtualMachineFactory
{
public:
    explicit QemuVirtualMachineFactory(const Path& data_dir);

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                const SSHKeyProvider& key_provider,
                                                VMStatusMonitor& monitor) override;
    VirtualMachine::UPtr create_vm_and_instance_disk_data(const QString& data_directory,
                                                          const VMSpecs& src_vm_spec,
                                                          const VMSpecs& dest_vm_spec,
                                                          const std::string& source_name,
                                                          const std::string& destination_name,
                                                          const VMImage& dest_vm_image,
                                                          VMStatusMonitor& monitor);

    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image, const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override;
    QString get_backend_version_string() const override;
    QString get_backend_directory_name() const override;
    std::vector<NetworkInterfaceInfo> networks() const override;
    void require_snapshots_support() const override;
    void prepare_networking(std::vector<NetworkInterface>& extra_interfaces) override;

protected:
    void remove_resources_for_impl(const std::string& name) override;

private:
    QemuVirtualMachineFactory(QemuPlatform::UPtr qemu_platform, const Path& data_dir);

    QemuPlatform::UPtr qemu_platform;
};
} // namespace multipass

inline void multipass::QemuVirtualMachineFactory::require_snapshots_support() const
{
}

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_FACTORY_H
