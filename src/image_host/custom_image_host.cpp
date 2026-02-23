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
#include <multipass/exceptions/image_not_found_exception.h>
#include <multipass/image_host/custom_image_host.h>
#include <multipass/logging/log.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "custom_image_host";
constexpr auto no_remote{""};
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
        if (!distro_obj.value("items").toObject().contains(arch))
        {
            mpl::debug(category,
                       "Skipping unsupported distro '{}' for arch '{}'",
                       distro_name,
                       arch);
            continue;
        }

        QStringList aliases = distro_obj.value("aliases").toString().split(",", Qt::SkipEmptyParts);
        for (QString& alias : aliases)
            alias = alias.trimmed();

        images.emplace_back(mp::VMImageInfo{aliases,
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

mp::CustomManifest::CustomManifest(std::vector<VMImageInfo>&& images)
    : products{std::move(images)}, image_records{map_aliases_to_vm_info(products)}
{
}

mp::CustomVMImageHost::CustomVMImageHost(URLDownloader* downloader)
    : BaseVMImageHost{downloader},
      arch{QSysInfo::currentCpuArchitecture()},
      manifest{},
      remote{no_remote}
{
}

std::optional<mp::VMImageInfo> mp::CustomVMImageHost::info_for(const Query& query)
{
    auto custom_manifest = manifest_from(query.remote_name);

    auto it = custom_manifest->image_records.find(query.release);

    if (it == custom_manifest->image_records.end())
        return std::nullopt;

    return *it->second;
}

std::vector<std::pair<std::string, mp::VMImageInfo>> mp::CustomVMImageHost::all_info_for(
    const Query& query)
{
    std::vector<std::pair<std::string, mp::VMImageInfo>> images;

    if (auto image = info_for(query))
        images.emplace_back(query.remote_name, std::move(*image));

    return images;
}

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_images_for(const std::string& remote_name)
{
    if (auto custom_manifest = manifest_from(remote_name))
        return custom_manifest->products;

    return {};
}

std::vector<std::string> mp::CustomVMImageHost::supported_remotes()
{
    return {remote};
}

void mp::CustomVMImageHost::for_each_entry_do_impl(const Action& action)
{
    for (const auto& info : manifest.second->products)
    {
        action(manifest.first, info);
    }
}

mp::VMImageInfo mp::CustomVMImageHost::info_for_full_hash_impl(const std::string& full_hash)
{
    for (const auto& product : manifest.second->products)
    {
        if (multipass::utils::iequals(product.id.toStdString(), full_hash))
        {
            return product;
        }
    }

    throw mp::ImageNotFoundException(full_hash);
}

void mp::CustomVMImageHost::fetch_manifests(const bool force_update)
{
    try
    {
        auto custom_manifest = std::make_unique<mp::CustomManifest>(
            fetch_image_info(arch, url_downloader, force_update));
        manifest = std::make_pair(no_remote, std::move(custom_manifest));
    }
    catch (mp::DownloadException& e)
    {
        throw e;
    }
}

void mp::CustomVMImageHost::clear()
{
    manifest = std::pair<std::string, std::unique_ptr<CustomManifest>>{};
}

mp::CustomManifest* mp::CustomVMImageHost::manifest_from(const std::string& remote_name)
{
    if (remote_name != manifest.first)
        throw std::runtime_error(
            fmt::format("Remote \"{}\" is unknown or unreachable.", remote_name));

    return manifest.second.get();
}
