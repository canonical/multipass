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

#pragma once

#include <multipass/path.h>
#include <shared/base_virtual_machine_factory.h>

namespace multipass::apple
{
class AppleVirtualMachineFactory final : public BaseVirtualMachineFactory
{
    explicit AppleVirtualMachineFactory(const Path& data_dir);

    [[nodiscard]] VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                              const SSHKeyProvider& key_provider,
                                                              VMStatusMonitor& monitor) override;

    [[nodiscard]] VMImage prepare_source_image(const VMImage& source_image) override;
    void prepare_instance_image(const VMImage& instance_image,
                                const VirtualMachineDescription& desc) override;
    void hypervisor_health_check() override;

    [[nodiscard]] QString get_backend_version_string() const override
    {
        return "apple";
    };

protected:
    void remove_resources_for_impl(const std::string& name) override;

};
} // namespace multipass::apple
