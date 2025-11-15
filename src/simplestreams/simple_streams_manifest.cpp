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

#include <multipass/simple_streams_manifest.h>

#include <QFileInfo>
#include <QHash>
#include <QSysInfo>

#include <boost/json.hpp>

#include <multipass/constants.h>
#include <multipass/exceptions/manifest_exceptions.h>
#include <multipass/settings/settings.h>
#include <multipass/utils.h>

namespace mp = multipass;

namespace
{
// Map QSysInfo::currentCpuArchitecture() to SimpleStreams manifest architecture names
const QHash<QString, QString> arch_to_manifest{{"x86_64", "amd64"},
                                               {"arm", "armhf"},
                                               {"power", "powerpc"},
                                               {"power64", "ppc64el"},
                                               {"power64le", "ppc64el"}};

const boost::json::object* if_contains_object(const boost::json::value& data, std::string_view key)
{
    if (auto elem = data.as_object().if_contains(key))
    {
        if (auto obj = elem->if_object())
            return obj;
    }
    return nullptr;
}

QString latest_version_in(const boost::json::object& versions)
{
    QString max_version;
    for (const auto& [key, _] : versions)
    {
        const auto version = QString::fromStdString(key);
        if (version < max_version)
            continue;
        max_version = version;
    }
    return max_version;
}

std::unordered_map<QString, const mp::VMImageInfo*> map_aliases_to_vm_info_for(
    const std::vector<mp::VMImageInfo>& images)
{
    std::unordered_map<QString, const mp::VMImageInfo*> map;

    for (const auto& image : images)
    {
        map[image.id] = &image;
        for (const auto& alias : image.aliases)
        {
            map[alias] = &image;
        }
    }

    return map;
}

} // namespace

mp::SimpleStreamsManifest::SimpleStreamsManifest(const QString& updated_at,
                                                 std::vector<VMImageInfo>&& images)
    : updated_at{updated_at},
      products{std::move(images)},
      image_records{map_aliases_to_vm_info_for(products)}
{
}

std::unique_ptr<mp::SimpleStreamsManifest> mp::SimpleStreamsManifest::fromJson(
    const QByteArray& json_from_official,
    const std::optional<QByteArray>& json_from_mirror,
    const QString& host_url,
    std::function<bool(VMImageInfo&)> mutator)
try
{
    // Get the official manifest products
    const auto manifest_from_official = boost::json::parse(std::string_view(json_from_official));
    const auto updated = lookup_or<QString>(manifest_from_official, "updated", "");
    const auto& manifest_products_from_official = manifest_from_official.at("products").as_object();
    if (manifest_products_from_official.empty())
        throw mp::GenericManifestException("No products found");

    auto arch = QSysInfo::currentCpuArchitecture();
    auto mapped_arch = arch_to_manifest.value(arch, arch);

    // Get the mirror manifest products, if any
    std::optional<boost::json::object> manifest_products_from_mirror = std::nullopt;
    if (json_from_mirror)
    {
        const auto manifest_from_mirror = boost::json::parse(std::string_view(*json_from_mirror));
        manifest_products_from_mirror = manifest_from_mirror.at("products").as_object();
    }
    const auto& manifest_products =
        manifest_products_from_mirror.value_or(manifest_products_from_official);

    std::vector<VMImageInfo> products;
    for (const auto& [product_key, product] : manifest_products)
    {
        if (lookup_or<QString>(product, "arch", "") != mapped_arch)
            continue;

        const auto* official_product = manifest_products_from_official.if_contains(product_key);
        if (!official_product)
            continue;

        auto product_aliases = lookup_or<QString>(product, "aliases", "").split(",");

        const auto image_type = lookup_or<QString>(product, "image_type", "");
        const auto os = lookup_or<QString>(product, "os", "");
        const auto release = lookup_or<QString>(product, "release", "");
        const auto release_title = lookup_or<QString>(product, "release_title", "");
        const auto release_codename = lookup_or<QString>(product, "release_codename", "");
        const auto supported = lookup_or<bool>(product, "supported", false) ||
                               product_aliases.contains("devel") ||
                               (os == "ubuntu-core" && image_type == "stable");

        const auto* versions = if_contains_object(product, "versions");
        if (!versions || versions->empty())
            continue;
        const auto* official_versions = if_contains_object(*official_product, "versions");
        if (!official_versions || official_versions->empty())
            continue;

        const auto latest_version = latest_version_in(*versions);
        for (const auto& [version_string, version] : *versions)
        {
            const auto* official_version = official_versions->if_contains(version_string);
            if (!official_version || version != *official_version)
                continue;

            const auto* items = if_contains_object(version, "items");
            if (!items || items->empty())
                continue;

            const auto& driver = MP_SETTINGS.get(mp::driver_key);

            QString sha256, image_location;
            int size = -1;

            std::string image_key;
            // Prioritize UEFI images
            if (items->contains("uefi1.img"))
                image_key = "uefi1.img";
            // For Ubuntu Core images
            else if (items->contains("img.xz") && os == "ubuntu-core")
                image_key = "img.xz";
            // Last resort, use img
            else if (items->contains("disk1.img"))
                image_key = "disk1.img";
            // Otherwise, give up
            else
                continue;

            const auto& image = items->at(image_key);
            image_location = host_url + value_to<QString>(image.at("path"));
            sha256 = lookup_or<QString>(image, "sha256", "");
            size = lookup_or<int>(image, "size", -1);

            const auto version_qstring = QString::fromStdString(version_string);
            // Aliases always alias to the latest version
            const QStringList& aliases =
                version_qstring == latest_version ? product_aliases : QStringList();

            VMImageInfo info{aliases,
                             "Ubuntu",
                             release,
                             release_title,
                             release_codename,
                             supported,
                             image_location,
                             sha256,
                             host_url,
                             version_qstring,
                             size,
                             true};

            if (mutator(info))
            {
                products.push_back(std::move(info));
            }
        }
    }
    if (products.empty())
        throw mp::EmptyManifestException("No supported products found.");

    return std::make_unique<SimpleStreamsManifest>(updated, std::move(products));
}
catch (const boost::system::system_error& e)
{
    throw GenericManifestException(e.what());
}
