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

#include "lxd_vm_image_vault.h"
#include "lxd_request.h"

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/format.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/url_downloader.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>

#include <QRegularExpression>

#include <chrono>
#include <thread>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace std::literals::chrono_literals;

namespace
{
constexpr auto category = "image vault";

auto parse_percent_as_int(const QString& progress_string)
{
    QRegularExpression re{"\\s(\\d{1,3})%"};

    QRegularExpressionMatch match = re.match(progress_string);

    if (match.hasMatch())
    {
        return match.captured(1).toInt();
    }

    return -1;
}
} // namespace

mp::LXDVMImageVault::LXDVMImageVault(std::vector<VMImageHost*> image_hosts)
    : image_hosts{image_hosts},
      base_url{"unix:///var/snap/lxd/common/lxd/unix.socket@1.0"},
      manager{std::make_unique<mp::NetworkAccessManager>()}
{
    for (const auto& image_host : image_hosts)
    {
        for (const auto& remote : image_host->supported_remotes())
        {
            remote_image_host_map[remote] = image_host;
        }
    }
}

mp::VMImage mp::LXDVMImageVault::fetch_image(const FetchType& fetch_type, const Query& query,
                                             const PrepareAction& prepare, const ProgressMonitor& monitor)
{
    const auto info = info_for(query);
    const auto id = info.id;
    VMImage source_image;

    source_image.id = id.toStdString();
    source_image.stream_location = info.stream_location.toStdString();
    source_image.original_release = info.release_title.toStdString();
    source_image.release_date = info.version.toStdString();

    for (const auto& alias : info.aliases)
    {
        source_image.aliases.push_back(alias.toStdString());
    }

    try
    {
        auto json_reply =
            lxd_request(manager.get(), "GET", QUrl(QString("%1/images/%2").arg(base_url.toString()).arg(id)));
    }
    catch (const LXDNotFoundException&)
    {
        QJsonObject image_object{{"source", QJsonObject{{"type", "image"},
                                                        {"mode", "pull"},
                                                        {"server", info.stream_location},
                                                        {"protocol", "simplestreams"},
                                                        {"fingerprint", id}}}};

        auto json_reply =
            lxd_request(manager.get(), "POST", QUrl(QString("%1/images").arg(base_url.toString())), image_object);

        if (json_reply["metadata"].toObject()["class"] == QStringLiteral("task") &&
            json_reply["status_code"].toInt(-1) == 100)
        {
            QUrl task_url(QString("%1/operations/%2")
                              .arg(base_url.toString())
                              .arg(json_reply["metadata"].toObject()["id"].toString()));

            // Instead of polling, need to use websockets to get events
            while (true)
            {
                try
                {
                    auto task_reply = lxd_request(manager.get(), "GET", task_url);

                    if (task_reply["error_code"].toInt(-1) != 0)
                    {
                        break;
                    }

                    auto status_code = task_reply["metadata"].toObject()["status_code"].toInt(-1);
                    if (status_code == 200)
                    {
                        break;
                    }
                    else
                    {
                        auto download_progress = parse_percent_as_int(
                            task_reply["metadata"].toObject()["metadata"].toObject()["download_progress"].toString());

                        if (!monitor(LaunchProgress::IMAGE, download_progress))
                        {
                            lxd_request(manager.get(), "DELETE", task_url);
                            throw mp::AbortedDownloadException{"Download aborted"};
                        }

                        std::this_thread::sleep_for(5s);
                    }
                }
                // Implies the task is finished
                catch (const LXDNotFoundException&)
                {
                    break;
                }
            }
        }
    }

    return source_image;
}

void mp::LXDVMImageVault::remove(const std::string& name)
{
    try
    {
        lxd_request(manager.get(), "DELETE",
                    QUrl(QString("%1/virtual-machines/%2").arg(base_url.toString()).arg(name.c_str())));
    }
    catch (const LXDNotFoundException&)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Instance \'{}\' does not exist: not removing", name));
    }
}

bool mp::LXDVMImageVault::has_record_for(const std::string& name)
{
    try
    {
        lxd_request(manager.get(), "GET",
                    QUrl(QString("%1/virtual-machines/%2").arg(base_url.toString()).arg(name.c_str())));

        return true;
    }
    catch (const LXDNotFoundException&)
    {
        return false;
    }
}

void mp::LXDVMImageVault::prune_expired_images()
{
}

void mp::LXDVMImageVault::update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                                        const ProgressMonitor& monitor)
{
}

mp::VMImageInfo mp::LXDVMImageVault::info_for(const mp::Query& query)
{
    if (!query.remote_name.empty())
    {
        auto it = remote_image_host_map.find(query.remote_name);
        if (it == remote_image_host_map.end())
            throw std::runtime_error(fmt::format("Remote \"{}\" is unknown.", query.remote_name));

        auto info = it->second->info_for(query);

        if (info != nullopt)
            return *info;
    }
    else
    {
        for (const auto& image_host : image_hosts)
        {
            auto info = image_host->info_for(query);

            if (info)
            {
                return *info;
            }
        }
    }

    throw std::runtime_error(fmt::format("Unable to find an image matching \"{}\"", query.release));
}
