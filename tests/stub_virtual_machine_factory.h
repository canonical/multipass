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

#ifndef MULTIPASS_STUB_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_STUB_VIRTUAL_MACHINE_FACTORY_H

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
    StubVirtualMachineFactory() : StubVirtualMachineFactory{std::make_unique<TempDir>()}
    {
    }

    StubVirtualMachineFactory(std::unique_ptr<TempDir> tmp_dir)
        : multipass::BaseVirtualMachineFactory{tmp_dir->path()}, tmp_dir{std::move(tmp_dir)}
    {
    }

    VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription&,
                                                const SSHKeyProvider&,
                                                VMStatusMonitor&) override
    {
        return std::make_unique<StubVirtualMachine>();
    }

    multipass::VirtualMachine::UPtr create_vm_and_instance_disk_data(const VMSpecs& src_vm_spec,
                                                                     const VMSpecs& dest_vm_spec,
                                                                     const std::string& source_name,
                                                                     const std::string& destination_name,
                                                                     const VMImage& dest_vm_image,
                                                                     const SSHKeyProvider& key_provider,
                                                                     VMStatusMonitor& monitor) override
    {
        return std::make_unique<StubVirtualMachine>();
    }

    void remove_resources_for_impl(const std::string& name) override
    {
    }

    multipass::FetchType fetch_type() override
    {
        return multipass::FetchType::ImageOnly;
    }

    multipass::VMImage prepare_source_image(const multipass::VMImage& source_image) override
    {
        return source_image;
    }

    void prepare_instance_image(const multipass::VMImage& instance_image,
                                const multipass::VirtualMachineDescription& vm_desc) override
    {
    }

    void hypervisor_health_check() override
    {
    }

    QString get_backend_directory_name() const override
    {
        return {};
    }

    QString get_instance_directory(const std::string& name) const override
    {
        return tmp_dir->path();
    }

    QString get_backend_version_string() const override
    {
        return "stub-5678";
    }

    multipass::VMImageVault::UPtr create_image_vault(std::vector<VMImageHost*> image_hosts, URLDownloader* downloader,
                                                     const Path& cache_dir_path, const Path& data_dir_path,
                                                     const days& days_to_expire) override
    {
        return std::make_unique<StubVMImageVault>();
    }

    void require_suspend_support() const override
    {
        throw NotImplementedOnThisBackendException{"suspend"};
    }

    std::unique_ptr<TempDir> tmp_dir;
};
}
}

#endif // MULTIPASS_STUB_VIRTUAL_MACHINE_FACTORY_H
