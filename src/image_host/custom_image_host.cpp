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

#include <boost/json.hpp>

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

std::vector<mp::VMImageInfo> fetch_image_info(const QString& arch,
                                              mp::URLDownloader* url_downloader,
                                              const bool force_update = false)
{
    mpl::log(mpl::Level::debug, category, "Fetching images from {}", get_manifest_url());

    try
    {
        auto data = url_downloader->download(QUrl{get_manifest_url()}, force_update);
        auto manifest = boost::json::parse(std::string_view(data)).as_object();
        mpl::log(mpl::Level::debug, category, "Found {} items", manifest.size());

        mp::for_arch context{arch.toStdString()};
        std::vector<mp::VMImageInfo> result;
        for (const auto& [distro_name, value] : manifest)
        {
            try
            {
                result.push_back(value_to<mp::VMImageInfo>(value, context));
            }
            catch (mp::UnsupportedArchException&)
            {
                mpl::debug(category,
                           "Skipping unsupported distro '{}' for arch '{}'",
                           distro_name,
                           arch);
                continue;
            }
        }
        return result;
    }
    catch (mp::DownloadException& e)
    {
        mpl::log(mpl::Level::warning, category, "Failed to download manifest: {}", e);
        return {};
    }
    catch (const boost::system::system_error&)
    {
        mpl::log(mpl::Level::warning,
                 category,
                 "Failed to parse manifest: file does not contain a valid JSON object");
        return {};
    }
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

std::vector<mp::VMImageInfo> mp::CustomVMImageHost::all_images_for(const std::string& remote_name,
                                                                   const bool allow_unsupported)
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
