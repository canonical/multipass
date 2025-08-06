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

#include <multipass/constants.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/image_host/multipass_image_host.h>
#include <multipass/logging/log.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>

#include <fmt/format.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "multipass_image_host";
constexpr auto manifest_endpoint{"https://raw.githubusercontent.com/canonical/multipass/refs/heads/"
                                 "main/data/distributions/distribution-info.json"};

auto get_manifest_url()
{
    return qEnvironmentVariable(mp::distributions_url_env_var).isEmpty()
               ? manifest_endpoint
               : qEnvironmentVariable(mp::distributions_url_env_var);
}

auto map_aliases_to_vm_info(const std::vector<mp::VMImageInfo>& images)
{
    std::unordered_map<std::string, const mp::VMImageInfo*> map;
    for (const auto& image : images)
    {
        map[image.id.toStdString()] = &image;
        for (const auto& alias : image.aliases)
        {
            map[alias.toStdString()] = &image;
        }
    }

    return map;
}

auto fetch_image_info(const QString& arch,
                      mp::URLDownloader* url_downloader,
                      const bool force_update = false)
{
    std::vector<mp::VMImageInfo> images;

    mpl::log(mpl::Level::debug, category, "Fetching images from {}", get_manifest_url());
    QByteArray mp_manifest;

    try
    {
        mp_manifest = url_downloader->download(QUrl{get_manifest_url()}, force_update);
    }
    catch (mp::DownloadException& e)
    {
        mpl::log(mpl::Level::warning, category, "Failed to download manifest: {}", e);
        return images;
    }

    const auto manifest_doc = QJsonDocument::fromJson(mp_manifest);
    if (!manifest_doc.isObject())
    {
        mpl::log(mpl::Level::warning,
                 category,
                 "Failed to parse manifest: file does not contain a valid JSON object");
        return images;
    }

    const QJsonObject root_obj = manifest_doc.object();

    mpl::log(mpl::Level::debug, category, "Found {} items", root_obj.size());

    for (auto it = root_obj.begin(); it != root_obj.end(); ++it)
    {
        const QString distro_name = it.key();
        const QJsonObject distro_obj = it.value().toObject();

        QStringList aliases = distro_obj.value("aliases").toString().split(",", Qt::SkipEmptyParts);
        for (QString& alias : aliases)
            alias = alias.trimmed();

        images.push_back(mp::VMImageInfo{aliases,
                                         distro_obj["os"].toString(),
                                         distro_obj["release"].toString(),
                                         distro_obj["release_codename"].toString(),
                                         distro_obj["release_title"].toString(),
                                         true,
                                         distro_obj["items"][arch]["image_location"].toString(),
                                         distro_obj["items"][arch]["id"].toString(),
                                         "",
                                         distro_obj["items"][arch]["version"].toString(),
                                         distro_obj["items"][arch]["size"].toInt(-1),
                                         true});
    }

    return images;
}
} // namespace

mp::MultipassManifest::MultipassManifest(std::vector<VMImageInfo>&& images)
    : products{std::move(images)}, image_records{map_aliases_to_vm_info(products)}
{
}

mp::MultipassVMImageHost::MultipassVMImageHost(URLDownloader* downloader)
    : BaseVMImageHost{downloader}, arch{QSysInfo::currentCpuArchitecture()}, manifest{}, remote{}
{
}

std::optional<mp::VMImageInfo> mp::MultipassVMImageHost::info_for(const Query& query)
{
    auto mp_manifest = manifest_from(query.remote_name);

    auto it = mp_manifest->image_records.find(query.release);

    if (it == mp_manifest->image_records.end())
        return std::nullopt;

    return *it->second;
}

std::vector<std::pair<std::string, mp::VMImageInfo>> mp::MultipassVMImageHost::all_info_for(
    const Query& query)
{
    std::vector<std::pair<std::string, mp::VMImageInfo>> images;

    auto image = info_for(query);
    if (image != std::nullopt)
        images.push_back(std::make_pair(query.remote_name, *image));

    return images;
}

std::vector<mp::VMImageInfo> mp::MultipassVMImageHost::all_images_for(
    const std::string& remote_name,
    const bool allow_unsupported)
{
    std::vector<mp::VMImageInfo> images;
    auto mp_manifest = manifest_from(remote_name);
    std::copy(mp_manifest->products.begin(),
              mp_manifest->products.end(),
              std::back_inserter(images));

    return images;
}

std::vector<std::string> mp::MultipassVMImageHost::supported_remotes()
{
    return {};
}

void mp::MultipassVMImageHost::for_each_entry_do_impl(const Action& action)
{
}

mp::VMImageInfo mp::MultipassVMImageHost::info_for_full_hash_impl(const std::string& full_hash)
{
    return {};
}

void mp::MultipassVMImageHost::fetch_manifests(const bool force_update)
{
}

void mp::MultipassVMImageHost::clear()
{
}
