/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_BASE_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_BASE_VIRTUAL_MACHINE_FACTORY_H

#include <multipass/cloud_init_iso.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include <daemon/default_vm_image_vault.h>

namespace multipass
{
constexpr auto log_category = "base factory";

class BaseVirtualMachineFactory : public VirtualMachineFactory
{
public:
    BaseVirtualMachineFactory() = default;

    FetchType fetch_type() override
    {
        return FetchType::ImageOnly;
    };

    QString get_backend_directory_name() override
    {
        return {};
    };

    VMImageVault::UPtr create_image_vault(std::vector<VMImageHost*> image_hosts, URLDownloader* downloader,
                                          const Path& cache_dir_path, const Path& data_dir_path,
                                          const days& days_to_expire) override
    {
        return std::make_unique<DefaultVMImageVault>(image_hosts, downloader, cache_dir_path, data_dir_path,
                                                     days_to_expire);
    };

    // By default, no network initialization data will be written to cloud-init, because cloud-init automatically
    // configures the default network interface. If many interfaces are present on an instance, then this function
    // must be overriden.
    Path make_cloud_init_image(const std::string& name, const QDir& instance_dir, YAML::Node& meta_data_config,
                               YAML::Node& user_data_config, YAML::Node& vendor_data_config,
                               YAML::Node& network_data_config) const override
    {
        const auto cloud_init_iso = instance_dir.filePath("cloud-init-config.iso");
        if (QFile::exists(cloud_init_iso))
            return cloud_init_iso;

        CloudInitIso iso;
        iso.add_file("meta-data", utils::emit_cloud_config(meta_data_config));
        iso.add_file("vendor-data", utils::emit_cloud_config(vendor_data_config));
        iso.add_file("user-data", utils::emit_cloud_config(user_data_config));
        iso.write_to(cloud_init_iso);

        return cloud_init_iso;
    }
};
} // namespace multipass

#endif // MULTIPASS_BASE_VIRTUAL_MACHINE_FACTORY_H
