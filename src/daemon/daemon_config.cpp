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

#include "daemon_config.h"
#include "default_vm_image_vault.h"
#include "ubuntu_image_host.h"

#include <multipass/logging/standard_logger.h>
#include <multipass/name_generator.h>
#include <multipass/platform.h>
#include <multipass/ssh/openssh_key_provider.h>

#include <QStandardPaths>

#include <iostream>

namespace mp = multipass;
namespace mpl = multipass::logging;

std::unique_ptr<const mp::DaemonConfig> mp::DaemonConfigBuilder::build()
{
    if (cache_directory.isEmpty())
        cache_directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (data_directory.isEmpty())
        data_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (logger == nullptr)
        logger = platform::make_logger(logging::Level::info);
    // Fallback when platform does not have a logger
    if (logger == nullptr)
        logger = std::make_unique<mpl::StandardLogger>(mpl::Level::info);
    if (url_downloader == nullptr)
        url_downloader = std::make_unique<URLDownloader>(cache_directory);
    if (factory == nullptr)
        factory = platform::vm_backend(data_directory);
    if (image_host == nullptr)
        image_host = std::make_unique<mp::UbuntuVMImageHost>(
            std::unordered_map<std::string, std::string>{
                {mp::release_remote, "http://cloud-images.ubuntu.com/releases/"},
                {mp::daily_remote, "http://cloud-images.ubuntu.com/daily/"}},
            url_downloader.get(), std::chrono::minutes{5});
    if (vault == nullptr)
        vault = std::make_unique<DefaultVMImageVault>(image_host.get(), url_downloader.get(), cache_directory,
                                                      data_directory, days_to_expire);
    if (name_generator == nullptr)
        name_generator = mp::make_default_name_generator();
    if (server_address.empty())
        server_address = platform::default_server_address();
    if (ssh_key_provider == nullptr)
        ssh_key_provider = std::make_unique<OpenSSHKeyProvider>(data_directory, cache_directory);
    if (ssh_username.empty())
        ssh_username = "multipass";

    return std::unique_ptr<const DaemonConfig>(
        new DaemonConfig{std::move(url_downloader), std::move(factory), std::move(image_host), std::move(vault),
                         std::move(name_generator), std::move(ssh_key_provider), std::move(logger), cache_directory,
                         data_directory, server_address, ssh_username});
}
