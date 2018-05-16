/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "default_vm_image_vault.h"
#include "json_writer.h"

#include <multipass/logging/log.h>
#include <multipass/query.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/url_downloader.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>

#include <fmt/format.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <exception>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "image vault";
constexpr auto instance_db_name = "multipassd-instance-image-records.json";
constexpr auto image_db_name = "multipassd-image-records.json";

auto filename_for(const QString& path)
{
    QFileInfo file_info(path);
    return file_info.fileName();
}

auto query_to_json(const mp::Query& query)
{
    QJsonObject json;
    json.insert("release", QString::fromStdString(query.release));
    json.insert("persistent", query.persistent);
    json.insert("remote_name", QString::fromStdString(query.remote_name));
    json.insert("query_type", static_cast<int>(query.query_type));
    return json;
}

auto image_to_json(const mp::VMImage& image)
{
    QJsonObject json;
    json.insert("path", image.image_path);
    json.insert("kernel_path", image.kernel_path);
    json.insert("initrd_path", image.initrd_path);
    json.insert("id", QString::fromStdString(image.id));
    json.insert("original_release", QString::fromStdString(image.original_release));
    json.insert("current_release", QString::fromStdString(image.current_release));

    QJsonArray aliases;
    for (const auto& alias : image.aliases)
    {
        QJsonObject alias_entry;
        alias_entry.insert("alias", QString::fromStdString(alias));
        aliases.append(alias_entry);
    }
    json.insert("aliases", aliases);

    return json;
}

auto record_to_json(const mp::VaultRecord& record)
{
    QJsonObject json;
    json.insert("image", image_to_json(record.image));
    json.insert("query", query_to_json(record.query));
    json.insert("last_accessed", static_cast<qint64>(record.last_accessed.time_since_epoch().count()));
    return json;
}

std::unordered_map<std::string, mp::VaultRecord> load_db(const QString& db_name, const QString& fallback_db_name=QString())
{
    QFile db_file{db_name};
    auto opened = db_file.open(QIODevice::ReadOnly);
    if (!opened)
    {
        if (fallback_db_name.isEmpty())
            return {};

        // Try to open the old location
        db_file.setFileName(fallback_db_name);
        auto opened = db_file.open(QIODevice::ReadOnly);
        if (!opened)
            return {};
    }

    QJsonParseError parse_error;
    auto doc = QJsonDocument::fromJson(db_file.readAll(), &parse_error);
    if (doc.isNull())
        return {};

    auto records = doc.object();
    if (records.isEmpty())
        return {};

    std::unordered_map<std::string, mp::VaultRecord> reconstructed_records;
    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        auto key = it.key().toStdString();
        auto record = it.value().toObject();
        if (record.isEmpty())
            return {};

        auto image = record["image"].toObject();
        if (image.isEmpty())
            return {};

        auto image_path = image["path"].toString();
        if (image_path.isNull())
            return {};

        auto kernel_path = image["kernel_path"].toString();
        auto initrd_path = image["initrd_path"].toString();
        auto image_id = image["id"].toString().toStdString();
        auto original_release = image["original_release"].toString().toStdString();
        auto current_release = image["current_release"].toString().toStdString();

        std::vector<std::string> aliases;
        for (const auto& entry : image["aliases"].toArray())
        {
            auto alias = entry.toObject()["alias"].toString().toStdString();
            aliases.push_back(alias);
        }

        auto query = record["query"].toObject();
        if (query.isEmpty())
            return {};

        auto release = query["release"].toString();
        auto persistent = query["persistent"];
        if (!persistent.isBool())
            return {};
        auto remote_name = query["remote_name"].toString();
        auto query_type = static_cast<mp::Query::Type>(query["type"].toInt());

        std::chrono::system_clock::time_point last_accessed;
        auto last_accessed_count = static_cast<qint64>(record["last_accessed"].toDouble());
        if (last_accessed_count == 0)
        {
            last_accessed = std::chrono::system_clock::now();
        }
        else
        {
            auto duration = std::chrono::system_clock::duration(last_accessed_count);
            last_accessed = std::chrono::system_clock::time_point(duration);
        }

        reconstructed_records[key] = {
            {image_path, kernel_path, initrd_path, image_id, original_release, current_release, aliases},
            {"", release.toStdString(), persistent.toBool(), remote_name.toStdString(), query_type},
            last_accessed};
    }
    return reconstructed_records;
}

QString copy(const QString& file_name, const QDir& output_dir)
{
    if (file_name.isEmpty())
        return {};

    QFileInfo info{file_name};
    const auto source_name = info.fileName();
    auto new_path = output_dir.filePath(source_name);
    QFile::copy(file_name, new_path);
    return new_path;
}

void delete_file(const QString& path)
{
    QFile file{path};
    file.remove();
}

void remove_source_images(const mp::VMImage& source_image, const mp::VMImage& prepared_image)
{
    // The prepare phase may have been a no-op, check and only remove source images
    if (source_image.image_path != prepared_image.image_path)
        delete_file(source_image.image_path);
    if (source_image.kernel_path != prepared_image.kernel_path)
        delete_file(source_image.kernel_path);
    if (source_image.initrd_path != prepared_image.initrd_path)
        delete_file(source_image.initrd_path);
}

QDir make_dir(const QString& name, const QDir& cache_dir)
{
    if (!cache_dir.mkpath(name))
    {
        QString dir{cache_dir.filePath(name)};
        throw std::runtime_error(fmt::format("unable to create directory '{}'", dir.toStdString()));
    }

    QDir new_dir{cache_dir};
    new_dir.cd(name);
    return new_dir;
}

class DeleteOnException
{
public:
    DeleteOnException(const mp::Path& path) : file(path)
    {
    }
    ~DeleteOnException()
    {
        if (std::uncaught_exception())
        {
            file.remove();
        }
    }

private:
    QFile file;
};
}

mp::DefaultVMImageVault::DefaultVMImageVault(VMImageHost* image_host, URLDownloader* downloader,
                                             mp::Path cache_dir_path, mp::Path data_dir_path, mp::days days_to_expire)
    : image_host{image_host},
      url_downloader{downloader},
      cache_dir{QDir(cache_dir_path).filePath("vault")},
      data_dir{QDir(data_dir_path).filePath("vault")},
      instances_dir(data_dir.filePath("instances")),
      images_dir(cache_dir.filePath("images")),
      days_to_expire{days_to_expire},
      prepared_image_records{load_db(cache_dir.filePath(image_db_name))},
      instance_image_records{load_db(data_dir.filePath(instance_db_name), cache_dir.filePath(instance_db_name))}
{
}

mp::VMImage mp::DefaultVMImageVault::fetch_image(const FetchType& fetch_type, const Query& query,
                                                 const PrepareAction& prepare, const ProgressMonitor& monitor)
{
    auto name_entry = instance_image_records.find(query.name);
    if (name_entry != instance_image_records.end())
    {
        const auto& record = name_entry->second;

        return record.image;
    }

    if (query.query_type != Query::Type::SimpleStreams)
    {
        QUrl image_url(QString::fromStdString(query.release));
        VMImage source_image;

        if (image_url.isLocalFile())
        {
            if (!QFile::exists(image_url.path()))
                throw std::runtime_error(
                    fmt::format("Custom image `{}` does not exist.", image_url.path().toStdString()));

            source_image.image_path = image_url.path();

            source_image = image_instance_from(query.name, prepare(source_image));
        }
        else
        {
            auto instance_dir = make_dir(QString::fromStdString(query.name), instances_dir);
            source_image.image_path = instance_dir.filePath(filename_for(image_url.path()));
            DeleteOnException image_file{source_image.image_path};

            url_downloader->download_to(image_url, source_image.image_path, 0, DownloadProgress::IMAGE, monitor);

            prepare(source_image);
        }

        instance_image_records[query.name] = {source_image, query, std::chrono::system_clock::now()};
        persist_instance_records();
        return source_image;
    }
    else
    {
        auto info = image_host->info_for(query);
        auto id = info.id.toStdString();

        if (!query.name.empty())
        {
            for (auto& record : prepared_image_records)
            {
                const auto aliases = record.second.image.aliases;
                if (id == record.first || std::find(aliases.cbegin(), aliases.cend(), query.release) != aliases.cend())
                {
                    const auto prepared_image = record.second.image;
                    auto vm_image = image_instance_from(query.name, prepared_image);
                    instance_image_records[query.name] = {vm_image, query, std::chrono::system_clock::now()};
                    persist_instance_records();
                    record.second.last_accessed = std::chrono::system_clock::now();
                    persist_image_records();
                    return vm_image;
                }
            }
        }

        auto image_dir_name = QString("%1-%2").arg(info.release).arg(info.version);
        auto image_dir = make_dir(image_dir_name, images_dir);

        VMImage source_image;
        source_image.id = id;
        source_image.image_path = image_dir.filePath(filename_for(info.image_location));
        source_image.original_release = info.release_title.toStdString();
        for (const auto& alias : info.aliases)
        {
            source_image.aliases.push_back(alias.toStdString());
        }
        DeleteOnException image_file{source_image.image_path};

        url_downloader->download_to(info.image_location, source_image.image_path, info.size, DownloadProgress::IMAGE,
                                    monitor);

        if (fetch_type == FetchType::ImageKernelAndInitrd)
        {
            source_image.kernel_path = image_dir.filePath(filename_for(info.kernel_location));
            source_image.initrd_path = image_dir.filePath(filename_for(info.initrd_location));
            DeleteOnException kernel_file{source_image.kernel_path};
            DeleteOnException initrd_file{source_image.initrd_path};
            url_downloader->download_to(info.kernel_location, source_image.kernel_path, -1, DownloadProgress::KERNEL,
                                        monitor);
            url_downloader->download_to(info.initrd_location, source_image.initrd_path, -1, DownloadProgress::INITRD,
                                        monitor);
        }

        auto prepared_image = prepare(source_image);
        prepared_image_records[id] = {prepared_image, query, std::chrono::system_clock::now()};
        remove_source_images(source_image, prepared_image);

        VMImage vm_image;

        if (!query.name.empty())
        {
            vm_image = image_instance_from(query.name, prepared_image);
            instance_image_records[query.name] = {vm_image, query, std::chrono::system_clock::now()};
        }

        persist_image_records();
        persist_instance_records();

        return vm_image;
    }
}

void mp::DefaultVMImageVault::remove(const std::string& name)
{
    auto name_entry = instance_image_records.find(name);
    if (name_entry == instance_image_records.end())
        return;

    QDir instance_dir{instances_dir};
    instance_dir.cd(QString::fromStdString(name));
    instance_dir.removeRecursively();

    instance_image_records.erase(name);
    persist_instance_records();
}

bool mp::DefaultVMImageVault::has_record_for(const std::string& name)
{
    return instance_image_records.find(name) != instance_image_records.end();
}

void mp::DefaultVMImageVault::prune_expired_images()
{
    std::vector<decltype(prepared_image_records)::key_type> expired_keys;
    for (const auto& record : prepared_image_records)
    {
        // Expire source images if they aren't persistent and haven't been accessed in 14 days
        if (!record.second.query.persistent &&
            record.second.last_accessed + days_to_expire <= std::chrono::system_clock::now())
        {
            mpl::log(
                mpl::Level::info, category,
                fmt::format("Source image {} is expired. Removing it from the cache.\n", record.second.query.release));
            expired_keys.push_back(record.first);
            QFileInfo image_file{record.second.image.image_path};
            if (image_file.exists())
                image_file.dir().removeRecursively();
        }
    }

    for (const auto& key : expired_keys)
        prepared_image_records.erase(key);

    persist_image_records();
}

void mp::DefaultVMImageVault::update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                                            const ProgressMonitor& monitor)
{
    std::vector<decltype(prepared_image_records)::key_type> keys_to_update;
    for (const auto& record : prepared_image_records)
    {
        if (record.first.compare(0, record.second.query.release.length(), record.second.query.release) != 0)
        {
            auto info = image_host->info_for(record.second.query);
            if (info.id.toStdString() != record.first)
            {
                keys_to_update.push_back(record.first);
            }
        }
    }

    for (const auto& key : keys_to_update)
    {
        const auto& record = prepared_image_records[key];
        mpl::log(mpl::Level::info, category, fmt::format("Updating {} source image to latest", record.query.release));
        fetch_image(fetch_type, record.query, prepare, monitor);
    }
}

mp::VMImage mp::DefaultVMImageVault::image_instance_from(const std::string& instance_name,
                                                         const VMImage& prepared_image)
{
    auto name = QString::fromStdString(instance_name);
    auto output_dir = make_dir(name, instances_dir);

    return {copy(prepared_image.image_path, output_dir),
            copy(prepared_image.kernel_path, output_dir),
            copy(prepared_image.initrd_path, output_dir),
            prepared_image.id,
            prepared_image.original_release,
            prepared_image.current_release,
            {}};
}

void mp::DefaultVMImageVault::persist_instance_records()
{
    QJsonObject instance_records_json;
    for (const auto& record : instance_image_records)
    {
        auto key = QString::fromStdString(record.first);
        instance_records_json.insert(key, record_to_json(record.second));
    }
    write_json(instance_records_json, data_dir.filePath(instance_db_name));
}

void mp::DefaultVMImageVault::persist_image_records()
{
    QJsonObject image_records_json;
    for (const auto& record : prepared_image_records)
    {
        auto key = QString::fromStdString(record.first);
        image_records_json.insert(key, record_to_json(record.second));
    }
    write_json(image_records_json, cache_dir.filePath(image_db_name));
}
