/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "daemon_config.h"
#include "default_vm_image_vault.h"

#include "custom_image_host.h"
#include "ubuntu_image_host.h"

#include <multipass/client_cert_store.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>
#include <multipass/name_generator.h>
#include <multipass/platform.h>
#include <multipass/ssh/openssh_key_provider.h>
#include <multipass/ssl_cert_provider.h>
#include <multipass/utils.h>

#include <QStandardPaths>

#include <chrono>
#include <memory>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto manifest_ttl = std::chrono::minutes{5};

std::string server_name_from(const std::string& server_address)
{
    auto tokens = mp::utils::split(server_address, ":");
    const auto server_name = tokens[0];

    if (server_name == "unix")
        return "localhost";
    return server_name;
}
} // namespace

mp::DaemonConfig::~DaemonConfig()
{
    mpl::set_logger(nullptr);
}

std::unique_ptr<const mp::DaemonConfig> mp::DaemonConfigBuilder::build()
{
    // Install logger as early as possible
    if (logger == nullptr)
        logger = platform::make_logger(verbosity_level);
    // Fallback when platform does not have a logger
    if (logger == nullptr)
        logger = std::make_unique<mpl::StandardLogger>(verbosity_level);

    auto multiplexing_logger = std::make_shared<mpl::MultiplexingLogger>(std::move(logger));
    mpl::set_logger(multiplexing_logger);

    if (cache_directory.isEmpty())
        cache_directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (data_directory.isEmpty())
        data_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (url_downloader == nullptr)
        url_downloader = std::make_unique<URLDownloader>(cache_directory, std::chrono::seconds{10});
    if (factory == nullptr)
        factory = platform::vm_backend(data_directory);
    if (image_hosts.empty())
    {
        image_hosts.push_back(std::make_unique<mp::CustomVMImageHost>(url_downloader.get(), manifest_ttl));
        image_hosts.push_back(std::make_unique<mp::UbuntuVMImageHost>(
            std::vector<std::pair<std::string, std::string>>{
                {mp::release_remote, "http://cloud-images.ubuntu.com/releases/"},
                {mp::daily_remote, "http://cloud-images.ubuntu.com/daily/"}},
            url_downloader.get(), manifest_ttl));
    }
    if (vault == nullptr)
    {
        std::vector<VMImageHost*> hosts;
        for (const auto& image : image_hosts)
        {
            hosts.push_back(image.get());
        }
        vault = std::make_unique<DefaultVMImageVault>(hosts, url_downloader.get(), cache_directory, data_directory,
                                                      days_to_expire);
    }
    if (name_generator == nullptr)
        name_generator = mp::make_default_name_generator();
    if (server_address.empty())
        server_address = platform::default_server_address();
    if (ssh_key_provider == nullptr)
        ssh_key_provider = std::make_unique<OpenSSHKeyProvider>(data_directory);
    if (cert_provider == nullptr)
        cert_provider = std::make_unique<mp::SSLCertProvider>(mp::utils::make_dir(data_directory, "certificates"),
                                                              server_name_from(server_address));
    if (client_cert_store == nullptr)
        client_cert_store =
            std::make_unique<mp::ClientCertStore>(mp::utils::make_dir(data_directory, "registered-certs"));
    if (ssh_username.empty())
        ssh_username = "multipass";

    return std::unique_ptr<const DaemonConfig>(
        new DaemonConfig{std::move(url_downloader), std::move(factory), std::move(image_hosts), std::move(vault),
                         std::move(name_generator), std::move(ssh_key_provider), std::move(cert_provider),
                         std::move(client_cert_store), multiplexing_logger, cache_directory,
                         data_directory, server_address, ssh_username, connection_type, image_refresh_timer});
}
