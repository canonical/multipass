/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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
#ifndef MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_FACTORY_H

#include "libvirt_wrapper.h"

#include <multipass/virtual_machine_factory.h>

#include <memory>
#include <string>

namespace multipass
{
class ProcessFactory;

class LibVirtVirtualMachineFactory final : public VirtualMachineFactory
{
public:
    explicit LibVirtVirtualMachineFactory(const Path& data_dir);
    ~LibVirtVirtualMachineFactory();

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                VMStatusMonitor& monitor) override;
    void remove_resources_for(const std::string& name) override;
    FetchType fetch_type() override;
    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image, const VirtualMachineDescription& desc) override;
    void configure(const std::string& name, YAML::Node& meta_config, YAML::Node& user_config) override;
    void hypervisor_health_check() override;
    QString get_backend_directory_name() override
    {
        return {};
    };
    QString get_backend_version_string() override;
    LibvirtWrapper libvirt_wrapper;

private:
    const Path data_dir;
    std::string bridge_name;
};
} // namespace multipass

#endif // MULTIPASS_LIBVIRT_VIRTUAL_MACHINE_FACTORY_H
