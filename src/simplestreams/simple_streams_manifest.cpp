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
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>

#include <multipass/constants.h>
#include <multipass/exceptions/manifest_exceptions.h>
#include <multipass/settings/settings.h>
#include <multipass/utils.h>

namespace mp = multipass;

namespace
{
const QHash<QString, QString> arch_to_manifest{{"x86_64", "amd64"}, {"arm", "armhf"},     {"arm64", "arm64"},
                                               {"i386", "i386"},    {"power", "powerpc"}, {"power64", "ppc64el"},
                                               {"s390x", "s390x"}};

QJsonObject parse_manifest(const QByteArray& json)
{
    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(json, &parse_error);
    if (doc.isNull())
        throw mp::GenericManifestException(parse_error.errorString().toStdString());

    if (!doc.isObject())
        throw mp::GenericManifestException("invalid manifest object");
    return doc.object();
}

QString latest_version_in(const QJsonObject& versions)
{
    QString max_version;
    for (auto it = versions.constBegin(); it != versions.constEnd(); ++it)
    {
        const auto version = it.key();
        if (version < max_version)
            continue;
        max_version = version;
    }
    return max_version;
}

QString derive_unpacked_file_path_prefix_from(const QString& image_location, const QString& image_suffix)
{
    QFileInfo info{image_location};
    auto file_name = info.fileName().remove('-' + image_suffix).remove(".img");
    return info.path().append("/unpacked/").append(file_name);
}
} // namespace

std::unique_ptr<mp::SimpleStreamsManifest>
mp::SimpleStreamsManifest::fromJson(const QByteArray& json_from_official,
                                    const std::optional<QByteArray>& json_from_mirror, const QString& host_url)
{
    const auto manifest_from_official = parse_manifest(json_from_official);
    const auto updated = manifest_from_official["updated"].toString();

    const auto manifest_products_from_official = manifest_from_official["products"].toObject();
    if (manifest_products_from_official.isEmpty())
        throw mp::GenericManifestException("No products found");

    auto arch = arch_to_manifest.value(QSysInfo::currentCpuArchitecture());

    if (arch.isEmpty())
        throw mp::GenericManifestException("Unsupported cloud image architecture");

    std::optional<QJsonObject> manifest_products_from_mirror = std::nullopt;
    if (json_from_mirror)
    {
        const auto manifest_from_mirror = parse_manifest(json_from_mirror.value());
        const auto products_from_mirror = manifest_from_mirror["products"].toObject();
        manifest_products_from_mirror = std::make_optional(products_from_mirror);
    }

    const QJsonObject manifest_products = manifest_products_from_mirror.value_or(manifest_products_from_official);

    std::vector<VMImageInfo> products;
    for (auto it = manifest_products.constBegin(); it != manifest_products.constEnd(); ++it)
    {
        const auto product_key = it.key();
        const auto product = it.value();

        if (product["arch"].toString() != arch)
            continue;

        const auto product_aliases = product["aliases"].toString().split(",");

        const auto release = product["release"].toString();
        const auto release_title = product["release_title"].toString();
        const auto supported = product["supported"].toBool();

        const auto versions = product["versions"].toObject();
        if (versions.isEmpty())
            continue;

        const auto latest_version = latest_version_in(versions);

        for (auto it = versions.constBegin(); it != versions.constEnd(); ++it)
        {
            const auto version_string = it.key();
            const auto version = versions[version_string].toObject();
            const auto version_from_official = manifest_products_from_official[product_key]
                                                   .toObject()["versions"]
                                                   .toObject()[version_string]
                                                   .toObject();

            if (version != version_from_official)
                continue;

            const auto items = version["items"].toObject();
            if (items.isEmpty())
                continue;

            const auto& driver = MP_SETTINGS.get(mp::driver_key);

            QJsonObject image;
            QString sha256, image_location, kernel_location, initrd_location;
            int size = -1;

            // TODO: make this a VM factory call with a preference list
            if (driver == "lxd")
            {
                image = items["lxd.tar.xz"].toObject();

                // Avoid kvm image due to canonical/multipass#2491
                if (image.contains("combined_disk1-img_sha256"))
                {
                    sha256 = image["combined_disk1-img_sha256"].toString();
                }
                else
                    continue;
            }
            else
            {
                const auto image_key = items.contains("uefi1.img") ? "uefi1.img" : "disk1.img";
                image = items[image_key].toObject();
                image_location = image["path"].toString();
                sha256 = image["sha256"].toString();
                size = image["size"].toInt(-1);

                // NOTE: These are not defined in the manifest itself
                // so they are not guaranteed to be correct or exist in the server
                const auto prefix = derive_unpacked_file_path_prefix_from(image_location, image_key);
                kernel_location = prefix + "-vmlinuz-generic";
                initrd_location = prefix + "-initrd-generic";
            }

            // Aliases always alias to the latest version
            const QStringList& aliases = version_string == latest_version ? product_aliases : QStringList();
            products.push_back({aliases, "Ubuntu", release, release_title, supported, image_location, kernel_location,
                                initrd_location, sha256, host_url, version_string, size, true});
        }
    }

    if (products.empty())
        throw mp::EmptyManifestException("No supported products found.");

    QMap<QString, const VMImageInfo*> map;

    for (const auto& product : products)
    {
        map[product.id] = &product;
        for (const auto& alias : product.aliases)
        {
            map[alias] = &product;
        }
    }

    return std::unique_ptr<SimpleStreamsManifest>(
        new SimpleStreamsManifest{updated, std::move(products), std::move(map)});
}
