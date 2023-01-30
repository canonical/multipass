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

#ifndef MULTIPASS_DAEMON_CONFIG_H
#define MULTIPASS_DAEMON_CONFIG_H

#include <multipass/cert_provider.h>
#include <multipass/cert_store.h>
#include <multipass/days.h>
#include <multipass/logging/logger.h>
#include <multipass/logging/multiplexing_logger.h>
#include <multipass/name_generator.h>
#include <multipass/path.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/update_prompt.h>
#include <multipass/url_downloader.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_blueprint_provider.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include <QNetworkProxy>

#include <memory>
#include <vector>

namespace multipass
{
struct DaemonConfig
{
    ~DaemonConfig();
    const std::unique_ptr<URLDownloader> url_downloader;
    const std::unique_ptr<VirtualMachineFactory> factory;
    const std::vector<std::unique_ptr<VMImageHost>> image_hosts;
    const std::unique_ptr<VMImageVault> vault;
    const std::unique_ptr<NameGenerator> name_generator;
    const std::unique_ptr<SSHKeyProvider> ssh_key_provider;
    const std::unique_ptr<CertProvider> cert_provider;
    const std::unique_ptr<CertStore> client_cert_store;
    const std::unique_ptr<UpdatePrompt> update_prompt;
    const std::shared_ptr<logging::MultiplexingLogger> logger;
    const std::unique_ptr<QNetworkProxy> network_proxy;
    const std::unique_ptr<VMBlueprintProvider> blueprint_provider;
    const multipass::Path cache_directory;
    const multipass::Path data_directory;
    const std::string server_address;
    const std::string ssh_username;
    const std::chrono::hours image_refresh_timer;
};

struct DaemonConfigBuilder
{
    std::unique_ptr<URLDownloader> url_downloader;
    std::unique_ptr<VirtualMachineFactory> factory;
    std::vector<std::unique_ptr<VMImageHost>> image_hosts;
    std::unique_ptr<VMImageVault> vault;
    std::unique_ptr<NameGenerator> name_generator;
    std::unique_ptr<SSHKeyProvider> ssh_key_provider;
    std::unique_ptr<CertProvider> cert_provider;
    std::unique_ptr<CertStore> client_cert_store;
    std::unique_ptr<UpdatePrompt> update_prompt;
    std::unique_ptr<logging::Logger> logger;
    std::unique_ptr<QNetworkProxy> network_proxy;
    std::unique_ptr<VMBlueprintProvider> blueprint_provider;
    multipass::Path cache_directory;
    multipass::Path data_directory;
    std::string server_address;
    std::string ssh_username;
    multipass::days days_to_expire{14};
    std::chrono::hours image_refresh_timer{6};
    multipass::logging::Level verbosity_level{multipass::logging::Level::info};

    std::unique_ptr<const DaemonConfig> build();
};
} // namespace multipass

#endif // MULTIPASS_DAEMON_CONFIG_H
