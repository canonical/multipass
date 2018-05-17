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

#ifndef MULTIPASS_DAEMON_CONFIG_H
#define MULTIPASS_DAEMON_CONFIG_H

#include <multipass/path.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <multipass/logging/logger.h>
#include <multipass/name_generator.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/url_downloader.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <memory>

namespace multipass
{

struct DaemonConfig
{
    const std::unique_ptr<URLDownloader> url_downloader;
    const std::unique_ptr<VirtualMachineFactory> factory;
    const std::unique_ptr<VMImageHost> image_host;
    const std::unique_ptr<VMImageVault> vault;
    const std::unique_ptr<NameGenerator> name_generator;
    const std::unique_ptr<SSHKeyProvider> ssh_key_provider;
    const std::shared_ptr<logging::Logger> logger;
    const multipass::Path cache_directory;
    const multipass::Path data_directory;
    const std::string server_address;
    const std::string ssh_username;
};

struct DaemonConfigBuilder
{
    std::unique_ptr<URLDownloader> url_downloader;
    std::unique_ptr<VirtualMachineFactory> factory;
    std::unique_ptr<VMImageHost> image_host;
    std::unique_ptr<VMImageVault> vault;
    std::unique_ptr<NameGenerator> name_generator;
    std::unique_ptr<SSHKeyProvider> ssh_key_provider;
    std::unique_ptr<logging::Logger> logger;
    multipass::Path cache_directory;
    multipass::Path data_directory;
    std::string server_address;
    std::string ssh_username;
    multipass::days days_to_expire{14};

    std::unique_ptr<const DaemonConfig> build();
};
}

#endif // MULTIPASS_DAEMON_CONFIG_H
