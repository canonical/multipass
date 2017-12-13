/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_HYPERV_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_HYPERV_VIRTUAL_MACHINE_FACTORY_H

#include <multipass/ip_address_pool.h>
#include <multipass/virtual_machine_factory.h>

namespace multipass
{
class HyperVVirtualMachineFactory final : public VirtualMachineFactory
{
public:
    explicit HyperVVirtualMachineFactory(const Path& data_dir);
    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                VMStatusMonitor& monitor) override;
    void remove_resources_for(const std::string& name) override;
    FetchType fetch_type() override;
    VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image, const VirtualMachineDescription& desc) override;
    void configure(const std::string& name, YAML::Node& meta_config, YAML::Node& user_config) override;
    void check_hypervisor_support() override;

private:
    IPAddressPool ip_pool;
};
}

#endif // MULTIPASS_HYPERV_VIRTUAL_MACHINE_FACTORY_H
