/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
        throw std::runtime_error(parse_error.errorString().toStdString());

    if (!doc.isObject())
        throw std::runtime_error("invalid manifest object");
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

QString derive_unpacked_file_path_prefix_from(const QString& image_location)
{
    QFileInfo info{image_location};
    auto file_name = info.fileName();
    file_name.remove("-disk1.img");
    file_name.remove(".img");

    auto prefix = info.path();
    prefix.append("/unpacked/");
    prefix.append(file_name);

    return prefix;
}
}

std::unique_ptr<mp::SimpleStreamsManifest> mp::SimpleStreamsManifest::fromJson(const QByteArray& json)
{
    const auto manifest = parse_manifest(json);
    const auto updated = manifest["updated"].toString();

    const auto manifest_products = manifest["products"].toObject();
    if (manifest_products.isEmpty())
        throw std::runtime_error("No products found");

    auto arch = arch_to_manifest.value(QSysInfo::currentCpuArchitecture());

    if (arch.isEmpty())
        throw std::runtime_error("Unsupported cloud image architecture");

    std::vector<VMImageInfo> products;
    for (const auto& value : manifest_products)
    {
        const auto product = value.toObject();

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
            const auto items = version["items"].toObject();
            if (items.isEmpty())
                continue;

            const auto image = items["disk1.img"].toObject();
            const auto image_location = image["path"].toString();
            const auto sha256 = image["sha256"].toString();
            const auto size = image["size"].toInt(-1);

            // NOTE: These are not defined in the manifest itself
            // so they are not guaranteed to be correct or exist in the server
            const auto prefix = derive_unpacked_file_path_prefix_from(image_location);
            const auto kernel_location = prefix + "-vmlinuz-generic";
            const auto initrd_location = prefix + "-initrd-generic";

            // Aliases always alias to the latest version
            const QStringList& aliases = version_string == latest_version ? product_aliases : QStringList();
            products.push_back({aliases, "Ubuntu", release, release_title, supported, image_location, kernel_location,
                                initrd_location, sha256, version_string, size, true});
        }
    }

    if (products.empty())
        throw std::runtime_error("failed to parse any products");

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
