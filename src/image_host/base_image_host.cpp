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

#include <multipass/exceptions/image_not_found_exception.h>
#include <multipass/exceptions/unsupported_image_exception.h>
#include <multipass/format.h>
#include <multipass/image_host/base_image_host.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/utils.h>

#include <algorithm>
#include <cctype>
#include <ranges>
#include <set>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "VMImageHost";

auto make_alias_filter(const std::string& filter)
{
    const auto is_number = mp::utils::has_only_digits(filter);
    auto prefix = is_number ? filter + '.' : filter;

    return [&filter, prefix = std::move(prefix)](const std::string& alias) {
        return mp::utils::istarts_with(alias, prefix) || mp::utils::iequals(alias, filter);
    };
}

auto make_image_filter(const mp::SearchQuery& query)
{
    return [&query, found_hashes = std::set<std::string>{}](const mp::VMImageInfo& info) mutable {
        if (!info.supported && !query.allow_unsupported)
        {
            return false;
        }

        if (found_hashes.contains(info.id))
        {
            return false;
        }

        if (!query.filter.empty())
        {
            auto alias_filter = make_alias_filter(query.filter);
            if (std::ranges::none_of(info.aliases, alias_filter))
            {
                return false;
            }
        }

        found_hashes.insert(info.id);
        return true;
    };
}

} // namespace

mp::BaseVMImageHost::BaseVMImageHost(URLDownloader* downloader) : url_downloader(downloader)
{
}

auto mp::BaseVMImageHost::info_for(const SearchQuery& query) const -> std::optional<VMImageInfo>
{
    // Ensure we're searching using a filter.
    auto with_filter = query;
    with_filter.filter = query.filter.empty() ? "default" : query.filter;

    // If no images are found, remove the filter.
    auto images = all_info_for(with_filter);
    if (images.empty() && query.filter.empty())
    {
        images = all_info_for(query);
    }

    // If still no images are found, check if the user forgot to allow
    // unsupported images.
    if (images.empty())
    {
        auto with_unsupported = query;
        with_unsupported.allow_unsupported = true;

        if (!query.allow_unsupported && !all_info_for(with_unsupported).empty())
        {
            throw UnsupportedImageException(query.filter);
        }

        return std::nullopt;
    }

    return images.front();
}

auto mp::BaseVMImageHost::all_info_for(const SearchQuery& query) const -> std::vector<VMImageInfo>
{
    std::shared_lock lock{manifest_mutex};

    const auto* images = images_for_remote(query.remote_name);
    if (images == nullptr)
    {
        throw std::runtime_error(
            fmt::format("Remote \"{}\" is unknown or unreachable", query.remote_name));
    }

    auto result = std::vector<VMImageInfo>{};

    auto match_query = make_image_filter(query);
    for (const auto& image : *images)
    {
        if (match_query(image))
        {
            result.emplace_back(image);
        }
    }

    return result;
}

auto mp::BaseVMImageHost::info_for_full_hash(const std::string& full_hash) const -> VMImageInfo
{
    std::shared_lock lock{manifest_mutex};

    for (const auto& remote : supported_remotes())
    {
        const auto* images = images_for_remote(remote);
        if (images != nullptr)
        {
            auto it = std::ranges::find_if(*images, [&full_hash](const VMImageInfo& image) {
                return multipass::utils::iequals(image.id, full_hash);
            });

            if (it != images->end())
            {
                return *it;
            }
        }
    }

    throw mp::ImageNotFoundException(full_hash);
}

void mp::BaseVMImageHost::for_each_entry_do(const Action& action) const
{
    std::shared_lock lock{manifest_mutex};

    for (const auto& remote : supported_remotes())
    {
        const auto* images = images_for_remote(remote);
        if (images != nullptr)
        {
            for (const auto& image : *images)
            {
                action(remote, image);
            }
        }
    }
}

void mp::BaseVMImageHost::update_manifests(bool force_update)
{
    std::lock_guard lock{manifest_mutex};
    clear();
    fetch_manifests(force_update);
}

void mp::BaseVMImageHost::on_manifest_empty(const std::string& details)
{
    mpl::log_message(mpl::Level::info, category, details);
}

void mp::BaseVMImageHost::on_manifest_update_failure(const std::string& details)
{
    mpl::warn(category, "Could not update manifest: {}", details);
}
