/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include "lxd_virtual_machine_factory.h"
#include "lxd_virtual_machine.h"
#include "lxd_vm_image_vault.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface_info.h>
#include <multipass/utils.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "lxd factory";
const QString multipass_bridge_name = "mpbr0";
} // namespace

mp::LXDVirtualMachineFactory::LXDVirtualMachineFactory(NetworkAccessManager::UPtr manager, const mp::Path& data_dir,
                                                       const QUrl& base_url)
    : manager{std::move(manager)},
      data_dir{mp::utils::make_dir(data_dir, get_backend_directory_name())},
      base_url{base_url}
{
}

mp::LXDVirtualMachineFactory::LXDVirtualMachineFactory(const mp::Path& data_dir, const QUrl& base_url)
    : LXDVirtualMachineFactory(std::make_unique<NetworkAccessManager>(), data_dir, base_url)
{
}

mp::VirtualMachine::UPtr mp::LXDVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                              VMStatusMonitor& monitor)
{
    return std::make_unique<mp::LXDVirtualMachine>(desc, monitor, manager.get(), base_url);
}

void mp::LXDVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    mpl::log(mpl::Level::trace, category, fmt::format("No resources to remove for \"{}\"", name));
}

void mp::LXDVirtualMachineFactory::prepare_instance_image(const mp::VMImage& /* instance_image */,
                                                          const VirtualMachineDescription& /* desc */)
{
    mpl::log(mpl::Level::trace, category, "No driver preparation for instance image");
}

void mp::LXDVirtualMachineFactory::hypervisor_health_check()
{
    auto reply = lxd_request(manager.get(), "GET", base_url);

    if (reply["metadata"].toObject()["auth"] != QStringLiteral("trusted"))
    {
        mpl::log(mpl::Level::debug, category, "Failed to authenticate to LXD:");
        mpl::log(mpl::Level::debug, category,
                 fmt::format("{}: {}", base_url.toString(), QJsonDocument(reply).toJson(QJsonDocument::Compact)));
        throw std::runtime_error("Failed to authenticate to LXD.");
    }

    try
    {
        lxd_request(manager.get(), "GET",
                    QUrl(QString("%1/projects/%2").arg(base_url.toString()).arg(lxd_project_name)));
    }
    catch (const LXDNotFoundException&)
    {
        QJsonObject project{{"name", lxd_project_name}, {"description", "Project for Multipass instances"}};

        lxd_request(manager.get(), "POST", QUrl(QString("%1/projects").arg(base_url.toString())), project);

        // TODO: Detect if default storage pool is available and if not, create a directory based pool for
        //       Multipass

        QJsonObject devices{
            {"eth0", QJsonObject{{"name", "eth0"}, {"nictype", "bridged"}, {"parent", "mpbr0"}, {"type", "nic"}}}};
        QJsonObject profile{{"description", "Default profile for Multipass project"}, {"devices", devices}};

        lxd_request(manager.get(), "PUT", QUrl(QString("%1/profiles/default").arg(base_url.toString())), profile);
    }

    try
    {
        lxd_request(manager.get(), "GET",
                    QUrl(QString("%1/networks/%2").arg(base_url.toString()).arg(multipass_bridge_name)));
    }
    catch (const LXDNotFoundException&)
    {
        QJsonObject network{{"name", multipass_bridge_name}, {"description", "Network bridge for Multipass"}};

        lxd_request(manager.get(), "POST", QUrl(QString("%1/networks").arg(base_url.toString())), network);
    }
}

QString mp::LXDVirtualMachineFactory::get_backend_version_string()
{
    auto reply = lxd_request(manager.get(), "GET", base_url);

    return QString("lxd-%1").arg(reply["metadata"].toObject()["environment"].toObject()["server_version"].toString());
}

mp::VMImageVault::UPtr mp::LXDVirtualMachineFactory::create_image_vault(std::vector<mp::VMImageHost*> image_hosts,
                                                                        mp::URLDownloader* downloader,
                                                                        const mp::Path& cache_dir_path,
                                                                        const mp::Path& data_dir_path,
                                                                        const mp::days& days_to_expire)
{
    return std::make_unique<mp::LXDVMImageVault>(image_hosts, manager.get(), base_url, days_to_expire);
}

auto multipass::LXDVirtualMachineFactory::list_networks() -> std::vector<NetworkInterfaceInfo>
{
    return {}; // TODO
}
