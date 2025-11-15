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

QString latest_version_in(const boost::json::object& versions)
{
    QString max_version;
    for (const auto& [key, _] : versions)
    {
        auto version = QString::fromStdString(key);
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
{
    auto arch = arch_to_manifest.value(QSysInfo::currentCpuArchitecture());

    // Get the official manifest products.
    const auto manifest_from_official = boost::json::parse(std::string_view(json_from_official));
    const auto updated = value_to<QString>(manifest_from_official.at("updated"));
    const auto& manifest_products_from_official = manifest_from_official.at("products").as_object();
    if (manifest_products_from_official.empty())
        throw mp::GenericManifestException("No products found");

    // Get the mirror manifest products, if any.
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
        if (value_to<QString>(product.at("arch")) != arch)
            continue;

        auto product_aliases = value_to<QString>(product.at("aliases")).split(",");

        const auto release = value_to<QString>(product.at("release"));
        const auto release_title = value_to<QString>(product.at("release_title"));
        const auto release_codename = value_to<QString>(product.at("release_codename"));
        const auto supported =
            value_to<bool>(product.at("supported")) || product_aliases.contains("devel") ||
            (product.at("os") == "ubuntu-core" && product.at("image_type") == "stable");

        const auto& versions = product.at("versions").as_object();
        if (versions.empty())
            continue;

        const auto latest_version = latest_version_in(versions);
        for (const auto& [version_string, version] : versions)
        {
            const auto& version_from_official =
                manifest_products_from_official.at(product_key).at("versions").at(version_string);

            if (version != version_from_official)
                continue;

            const auto& items = version.at("items").as_object();
            if (items.empty())
                continue;

            const auto& driver = MP_SETTINGS.get(mp::driver_key);

            QString sha256, image_location;
            int size = -1;

            QString image_key;
            // Prioritize UEFI images
            if (items.contains("uefi1.img"))
                image_key = "uefi1.img";
            // For Ubuntu Core images
            else if (product.at("os") == "ubuntu-core" && items.contains("img.xz"))
                image_key = "img.xz";
            // Last resort, use img
            else
                image_key = "disk1.img";

            const auto& image = items.at(image_key.toStdString());
            image_location = host_url + value_to<QString>(image.at("path"));
            sha256 = value_to<QString>(image.at("sha256"));
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
