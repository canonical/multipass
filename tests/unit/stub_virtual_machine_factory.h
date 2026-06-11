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

#include "stub_virtual_machine.h"
#include "stub_vm_image_vault.h"
#include "temp_dir.h"

#include <platform/backends/shared/base_virtual_machine_factory.h>

namespace multipass
{
namespace test
{
struct StubVirtualMachineFactory : public multipass::BaseVirtualMachineFactory
{
    StubVirtualMachineFactory(AvailabilityZoneManager& az_manager)
        : StubVirtualMachineFactory{std::make_unique<TempDir>(), az_manager}
    {
    }

    StubVirtualMachineFactory(std::unique_ptr<TempDir> tmp_dir, AvailabilityZoneManager& az_manager)
        : multipass::BaseVirtualMachineFactory{tmp_dir->path(), az_manager},
          tmp_dir{std::move(tmp_dir)}
    {
    }

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription&,
                                                const SSHKeyProvider&,
                                                VMStatusMonitor&) override
    {
        return std::make_unique<StubVirtualMachine>();
    }

    void remove_resources_for_impl(const std::string&) override
    {
    }

    multipass::VMImage prepare_source_image(const multipass::VMImage& source_image) override
    {
        return source_image;
    }

    void prepare_instance_image(const multipass::VMImage&,
                                const multipass::VirtualMachineDescription&) override
    {
    }

    void hypervisor_health_check() override
    {
    }

    QString get_backend_directory_name() const override
    {
        return {};
    }

    std::filesystem::path get_instance_directory(const std::string&) const override
    {
        return *tmp_dir;
    }

    QString get_backend_version_string() const override
    {
        return "stub-5678";
    }

    multipass::VMImageVault::UPtr create_image_vault(
        std::vector<VMImageHost*> /*image_hosts*/,
        URLDownloader* /*downloader*/,
        const std::filesystem::path& /*cache_dir_path*/,
        const std::filesystem::path& /*data_dir_path*/,
        const days& /*days_to_expire*/) override
    {
        return std::make_unique<StubVMImageVault>();
    }

    std::unique_ptr<TempDir> tmp_dir;
};
} // namespace test
} // namespace multipass
