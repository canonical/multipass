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

#include "lxd_virtual_machine_factory.h"
#include "lxd_virtual_machine.h"
#include "lxd_vm_image_vault.h"

#include <shared/linux/backend_utils.h>

#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface.h>
#include <multipass/network_interface_info.h>
#include <multipass/platform.h>
#include <multipass/snap_utils.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "lxd factory";
const QString multipass_bridge_name = "mpbr0";

mp::NetworkInterfaceInfo munch_network(std::map<std::string, mp::NetworkInterfaceInfo>& platform_networks,
                                       const QJsonObject& network)
{
    using namespace std::string_literals;
    static const std::array supported_types{"bridge"s, "ethernet"s};

    mp::NetworkInterfaceInfo ret;
    if (auto qid = network["name"].toString(); !qid.isEmpty())
    {
        auto id = qid.toStdString();
        if (auto platform_it = platform_networks.find(id); platform_it != platform_networks.cend())
        {
            if (auto& type = platform_it->second.type;
                std::find(supported_types.cbegin(), supported_types.cend(), type) != supported_types.cend())
            {
                auto lxd_description = network["description"].toString();
                auto description = lxd_description.isEmpty() ? std::move(platform_it->second.description)
                                                             : lxd_description.toStdString();
                auto require_authorization = type != "bridge";

                ret = {std::move(id), std::move(type), std::move(description), std::move(platform_it->second.links),
                       require_authorization};

                platform_networks.erase(platform_it); // prevent matching with this network again
            }
        }
    }

    return ret;
}
} // namespace

mp::LXDVirtualMachineFactory::LXDVirtualMachineFactory(NetworkAccessManager::UPtr manager,
                                                       const mp::Path& data_dir,
                                                       const QUrl& base_url)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir)),
      manager{std::move(manager)},
      base_url{base_url}
{
}

mp::LXDVirtualMachineFactory::LXDVirtualMachineFactory(const mp::Path& data_dir, const QUrl& base_url)
    : LXDVirtualMachineFactory(std::make_unique<NetworkAccessManager>(), data_dir, base_url)
{
}

mp::VirtualMachine::UPtr mp::LXDVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                              const SSHKeyProvider& key_provider,
                                                                              VMStatusMonitor& monitor)
{
    return std::make_unique<mp::LXDVirtualMachine>(desc,
                                                   monitor,
                                                   manager.get(),
                                                   base_url,
                                                   multipass_bridge_name,
                                                   storage_pool,
                                                   key_provider,
                                                   MP_UTILS.make_dir(get_instance_directory(desc.vm_name)));
}

void mp::LXDVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
    mpl::log(mpl::Level::trace, category, fmt::format("No further resources to remove for \"{}\"", name));
}

auto mp::LXDVirtualMachineFactory::prepare_source_image(const VMImage& source_image) -> VMImage
{
    mpl::log(mpl::Level::trace, category, "No driver preparation required for source image");
    return source_image;
}

void mp::LXDVirtualMachineFactory::prepare_instance_image(const mp::VMImage& /* instance_image */,
                                                          const VirtualMachineDescription& /* desc */)
{
    mpl::log(mpl::Level::trace, category, "No driver preparation for instance image");
}

void mp::LXDVirtualMachineFactory::configure(VirtualMachineDescription& /*vm_desc*/)
{
    mpl::log(mpl::Level::trace, category, "No preliminary configure step in LXD driver");
}

void mp::LXDVirtualMachineFactory::hypervisor_health_check()
{
    QJsonObject reply;

    try
    {
        reply = lxd_request(manager.get(), "GET", base_url);
    }
    catch (const LocalSocketConnectionException& e)
    {
        std::string snap_msg;
        if (mpu::in_multipass_snap())
            snap_msg = " Also make sure\n the LXD interface is connected via `snap connect multipass:lxd lxd`.";

        throw std::runtime_error(
            fmt::format("{}\n\nPlease ensure the LXD snap is installed and enabled.{}", e.what(), snap_msg));
    }

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
    }

    const QStringList pools_to_try{{"multipass", "default"}};
    for (const auto& pool : pools_to_try)
    {
        try
        {
            lxd_request(manager.get(), "GET", QUrl(QString("%1/storage-pools/%2").arg(base_url.toString()).arg(pool)));

            storage_pool = pool;
            mpl::log(mpl::Level::debug, category, fmt::format("Using the \'{}\' storage pool.", pool));

            break;
        }
        catch (const LXDNotFoundException&)
        {
            // Keep going
        }
    }

    // No storage pool to use, so create a multipass dir-based pool
    if (storage_pool.isEmpty())
    {
        storage_pool = "multipass";
        mpl::log(mpl::Level::info, category, "No storage pool found for multpass: creating…");
        QJsonObject pool_config{
            {"description", "Storage pool for Multipass"}, {"name", storage_pool}, {"driver", "dir"}};
        lxd_request(manager.get(), "POST", QUrl(QString("%1/storage-pools").arg(base_url.toString())), pool_config);
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

QString mp::LXDVirtualMachineFactory::get_backend_version_string() const
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
    return std::make_unique<mp::LXDVMImageVault>(image_hosts, downloader, manager.get(), base_url, cache_dir_path,
                                                 days_to_expire);
}

auto mp::LXDVirtualMachineFactory::networks() const -> std::vector<NetworkInterfaceInfo>
{
    auto url = QUrl{QString{"%1/networks?recursion=1"}.arg(base_url.toString())}; // no network filter ATTOW
    auto reply = lxd_request(manager.get(), "GET", url);

    auto ret = std::vector<NetworkInterfaceInfo>{};
    auto networks = reply["metadata"].toArray();

    if (!networks.isEmpty())
    {
        const auto& br_nomenclature = MP_PLATFORM.bridge_nomenclature();
        auto platform_networks = MP_PLATFORM.get_network_interfaces_info();
        for (const QJsonValueRef net_value : networks)
            if (auto network = munch_network(platform_networks, net_value.toObject()); !network.id.empty())
                ret.push_back(std::move(network));

        for (auto& net : ret)
            if (net.needs_authorization && mpu::find_bridge_with(ret, net.id, br_nomenclature))
                net.needs_authorization = false;
    }

    return ret;
}

std::string mp::LXDVirtualMachineFactory::create_bridge_with(const NetworkInterfaceInfo& interface)
{
    assert(interface.type == "ethernet");
    return MP_BACKEND.create_bridge_with(interface.id);
}
