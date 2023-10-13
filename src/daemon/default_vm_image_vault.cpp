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

#include "default_vm_image_vault.h"

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/create_image_exception.h>
#include <multipass/exceptions/image_vault_exceptions.h>
#include <multipass/exceptions/unsupported_image_exception.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/query.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>
#include <multipass/vm_image.h>
#include <multipass/xz_image_decoder.h>

#include <multipass/format.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#include <exception>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "image vault";
constexpr auto instance_db_name = "multipassd-instance-image-records.json";
constexpr auto image_db_name = "multipassd-image-records.json";

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
    json.insert("id", QString::fromStdString(image.id));
    json.insert("original_release", QString::fromStdString(image.original_release));
    json.insert("current_release", QString::fromStdString(image.current_release));
    json.insert("release_date", QString::fromStdString(image.release_date));

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

std::unordered_map<std::string, mp::VaultRecord> load_db(const QString& db_name)
{
    QFile db_file{db_name};
    auto opened = db_file.open(QIODevice::ReadOnly);
    if (!opened)
        return {};

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

        auto image_id = image["id"].toString().toStdString();
        auto original_release = image["original_release"].toString().toStdString();
        auto current_release = image["current_release"].toString().toStdString();
        auto release_date = image["release_date"].toString().toStdString();

        std::vector<std::string> aliases;
        for (QJsonValueRef entry : image["aliases"].toArray())
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
            {image_path, image_id, original_release, current_release, release_date, aliases},
            {"", release.toStdString(), persistent.toBool(), remote_name.toStdString(), query_type},
            last_accessed};
    }
    return reconstructed_records;
}

void remove_source_images(const mp::VMImage& source_image, const mp::VMImage& prepared_image)
{
    // The prepare phase may have been a no-op, check and only remove source images
    if (source_image.image_path != prepared_image.image_path)
        mp::vault::delete_file(source_image.image_path);
}

void delete_image_dir(const mp::Path& image_path)
{
    QFileInfo image_file{image_path};
    if (image_file.exists())
    {
        if (image_file.isDir())
            QDir(image_path).removeRecursively();
        else
            image_file.dir().removeRecursively();
    }
}

mp::MemorySize get_image_size(const mp::Path& image_path)
{
    QStringList qemuimg_parameters{{"info", image_path}};
    auto qemuimg_process =
        mp::platform::make_process(std::make_unique<mp::QemuImgProcessSpec>(qemuimg_parameters, image_path));
    auto process_state = qemuimg_process->execute();

    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Cannot get image info: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(),
                                             qemuimg_process->read_all_standard_error()));
    }

    const auto img_info = QString{qemuimg_process->read_all_standard_output()};
    const auto pattern = QStringLiteral("^virtual size: .+ \\((?<size>\\d+) bytes\\)\r?$");
    const auto re = QRegularExpression{pattern, QRegularExpression::MultilineOption};

    mp::MemorySize image_size{};

    const auto match = re.match(img_info);

    if (match.hasMatch())
    {
        image_size = mp::MemorySize(match.captured("size").toStdString());
    }
    else
    {
        throw std::runtime_error{"Could not obtain image's virtual size"};
    }

    return image_size;
}

template <typename T>
void persist_records(const T& records, const QString& path)
{
    QJsonObject json_records;
    for (const auto& record : records)
    {
        auto key = QString::fromStdString(record.first);
        json_records.insert(key, record_to_json(record.second));
    }
    MP_JSONUTILS.write_json(json_records, path);
}
} // namespace

mp::DefaultVMImageVault::DefaultVMImageVault(std::vector<VMImageHost*> image_hosts,
                                             URLDownloader* downloader,
                                             const mp::Path& cache_dir_path,
                                             const mp::Path& data_dir_path,
                                             const mp::days& days_to_expire)
    : BaseVMImageVault{image_hosts},
      url_downloader{downloader},
      cache_dir{QDir(cache_dir_path).filePath("vault")},
      data_dir{QDir(data_dir_path).filePath("vault")},
      images_dir(cache_dir.filePath("images")),
      days_to_expire{days_to_expire},
      prepared_image_records{load_db(cache_dir.filePath(image_db_name))},
      instance_image_records{load_db(data_dir.filePath(instance_db_name))}
{
}

mp::DefaultVMImageVault::~DefaultVMImageVault()
{
    url_downloader->abort_all_downloads();
}

mp::VMImage mp::DefaultVMImageVault::fetch_image(const FetchType& fetch_type,
                                                 const Query& query,
                                                 const PrepareAction& prepare,
                                                 const ProgressMonitor& monitor,
                                                 const bool unlock,
                                                 const std::optional<std::string>& checksum,
                                                 const mp::Path& save_dir)
{
    {
        std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
        auto name_entry = instance_image_records.find(query.name);
        if (name_entry != instance_image_records.end())
        {
            const auto& record = name_entry->second;

            return record.image;
        }
    }

    if (!unlock && query.query_type != Query::Type::Alias && !MP_PLATFORM.is_image_url_supported())
        throw std::runtime_error(fmt::format("http and file based images are not supported"));

    if (query.query_type == Query::Type::LocalFile)
    {
        QUrl image_url(QString::fromStdString(query.release));
        VMImage source_image, vm_image;

        if (!QFile::exists(image_url.path()))
            throw std::runtime_error(fmt::format("Custom image `{}` does not exist.", image_url.path()));

        source_image.image_path = image_url.path();

        if (source_image.image_path.endsWith(".xz"))
        {
            source_image.image_path = extract_image_from(source_image, monitor, save_dir);
        }
        else
        {
            source_image = image_instance_from(source_image, save_dir);
        }

        vm_image = prepare(source_image);
        vm_image.id = mp::vault::compute_image_hash(vm_image.image_path).toStdString();

        remove_source_images(source_image, vm_image);

        {
            std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
            instance_image_records[query.name] = {vm_image, query, std::chrono::system_clock::now()};
            persist_instance_records();
        }

        return vm_image;
    }
    else
    {
        std::string id;
        std::optional<VMImage> source_image{std::nullopt};
        QFuture<VMImage> future;

        if (query.query_type == Query::Type::HttpDownload)
        {
            QUrl image_url(QString::fromStdString(query.release));

            // If no checksum given, generate a sha256 hash based on the URL and use that for the id
            id =
                checksum
                    ? *checksum
                    : QCryptographicHash::hash(query.release.c_str(), QCryptographicHash::Sha256).toHex().toStdString();
            auto last_modified = url_downloader->last_modified(image_url);

            std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
            auto entry = prepared_image_records.find(id);
            if (entry != prepared_image_records.end())
            {
                auto& record = entry->second;

                if (last_modified.isValid() && (last_modified.toString().toStdString() == record.image.release_date))
                {
                    return finalize_image_records(query, record.image, id, save_dir);
                }
            }

            auto running_future = get_image_future(id);
            if (running_future)
            {
                monitor(LaunchProgress::WAITING, -1);
                future = *running_future;
            }
            else
            {
                const VMImageInfo info{{},
                                       {},
                                       {},
                                       {},
                                       {},
                                       true,
                                       image_url.url(),
                                       QString::fromStdString(id),
                                       {},
                                       last_modified.toString(),
                                       0,
                                       checksum.has_value()};
                const auto image_filename = mp::vault::filename_for(image_url.path());
                // Attempt to make a sane directory name based on the filename of the image

                const auto image_dir_name =
                    QString("%1-%2").arg(image_filename.section(".", 0, image_filename.endsWith(".xz") ? -3 : -2),
                                         QLocale::c().toString(last_modified, "yyyyMMdd"));
                const auto image_dir = MP_UTILS.make_dir(images_dir, image_dir_name);

                // Had to use std::bind here to workaround the 5 allowable function arguments constraint of
                // QtConcurrent::run()
                future = QtConcurrent::run(std::bind(&DefaultVMImageVault::download_and_prepare_source_image, this,
                                                     info, source_image, image_dir, fetch_type, prepare, monitor));

                in_progress_image_fetches[id] = future;
            }
        }
        else
        {
            const auto info = info_for(query);
            if (!info)
                throw mp::ImageNotFoundException(query.release, query.remote_name);

            id = info->id.toStdString();

            std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
            if (!query.name.empty())
            {
                for (auto& record : prepared_image_records)
                {
                    if (record.second.query.remote_name != query.remote_name)
                        continue;

                    const auto aliases = record.second.image.aliases;
                    if (id == record.first ||
                        std::find(aliases.cbegin(), aliases.cend(), query.release) != aliases.cend())
                    {
                        const auto prepared_image = record.second.image;
                        try
                        {
                            return finalize_image_records(query, prepared_image, record.first, save_dir);
                        }
                        catch (const std::exception& e)
                        {
                            mpl::log(mpl::Level::warning, category,
                                     fmt::format("Cannot create instance image: {}", e.what()));

                            break;
                        }
                    }
                }
            }

            auto running_future = get_image_future(id);
            if (running_future)
            {
                monitor(LaunchProgress::WAITING, -1);
                future = *running_future;
            }
            else
            {
                const auto image_dir =
                    MP_UTILS.make_dir(images_dir, QString("%1-%2").arg(info->release).arg(info->version));

                // Had to use std::bind here to workaround the 5 allowable function arguments constraint of
                // QtConcurrent::run()
                future = QtConcurrent::run(std::bind(&DefaultVMImageVault::download_and_prepare_source_image, this,
                                                     *info, source_image, image_dir, fetch_type, prepare, monitor));

                in_progress_image_fetches[id] = future;
            }
        }

        try
        {
            auto prepared_image = future.result();
            std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
            in_progress_image_fetches.erase(id);
            return finalize_image_records(query, prepared_image, id, save_dir);
        }
        catch (const std::exception&)
        {
            std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
            in_progress_image_fetches.erase(id);
            throw;
        }
    }
}

void mp::DefaultVMImageVault::remove(const std::string& name)
{
    const auto& name_entry = instance_image_records.find(name);
    if (name_entry == instance_image_records.end())
        return;

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
    std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};

    for (const auto& record : prepared_image_records)
    {
        // Expire source images if they aren't persistent and haven't been accessed in 14 days
        if (record.second.query.query_type == Query::Type::Alias && !record.second.query.persistent &&
            record.second.last_accessed + days_to_expire <= std::chrono::system_clock::now())
        {
            mpl::log(
                mpl::Level::info, category,
                fmt::format("Source image {} is expired. Removing it from the cache.", record.second.query.release));
            expired_keys.push_back(record.first);
            delete_image_dir(record.second.image.image_path);
        }
    }

    // Remove any image directories that have no corresponding database entry
    for (const auto& entry : images_dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot))
    {
        if (std::find_if(prepared_image_records.cbegin(), prepared_image_records.cend(),
                         [&entry](const std::pair<std::string, VaultRecord>& record) {
                             return record.second.image.image_path.contains(entry.absoluteFilePath());
                         }) == prepared_image_records.cend())
        {
            mpl::log(mpl::Level::info, category,
                     fmt::format("Source image {} is no longer valid. Removing it from the cache.",
                                 entry.absoluteFilePath()));
            delete_image_dir(entry.absoluteFilePath());
        }
    }

    for (const auto& key : expired_keys)
        prepared_image_records.erase(key);

    persist_image_records();
}

void mp::DefaultVMImageVault::update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                                            const ProgressMonitor& monitor)
{
    mpl::log(mpl::Level::debug, category, "Checking for images to update…");

    std::vector<decltype(prepared_image_records)::key_type> keys_to_update;
    for (const auto& record : prepared_image_records)
    {
        if (record.second.query.query_type == Query::Type::Alias &&
            record.first.compare(0, record.second.query.release.length(), record.second.query.release) != 0)
        {
            try
            {
                auto info = info_for(record.second.query);
                if (!info)
                    throw mp::ImageNotFoundException(record.second.query.release, record.second.query.remote_name);

                if (info->id.toStdString() != record.first)
                {
                    keys_to_update.push_back(record.first);
                }
            }
            catch (const mp::UnsupportedImageException& e)
            {
                mpl::log(mpl::Level::warning, category, fmt::format("Skipping update: {}", e.what()));
            }
            catch (const mp::ImageNotFoundException& e)
            {
                mpl::log(mpl::Level::warning, category, fmt::format("Skipping update: {}", e.what()));
            }
        }
    }

    for (const auto& key : keys_to_update)
    {
        const auto& record = prepared_image_records[key];
        mpl::log(mpl::Level::info, category, fmt::format("Updating {} source image to latest", record.query.release));
        try
        {
            fetch_image(fetch_type,
                        record.query,
                        prepare,
                        monitor,
                        false,
                        std::nullopt,
                        QFileInfo{record.image.image_path}.absolutePath());

            // Remove old image
            std::lock_guard<decltype(fetch_mutex)> lock{fetch_mutex};
            delete_image_dir(record.image.image_path);
            prepared_image_records.erase(key);
            persist_image_records();
        }
        catch (const CreateImageException& e)
        {
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Cannot update source image {}: {}", record.query.release, e.what()));
        }
    }
}

mp::MemorySize mp::DefaultVMImageVault::minimum_image_size_for(const std::string& id)
{
    auto prepared_image_entry = prepared_image_records.find(id);
    if (prepared_image_entry != prepared_image_records.end())
    {
        const auto& record = prepared_image_entry->second;

        return get_image_size(record.image.image_path);
    }

    for (const auto& instance_image_entry : instance_image_records)
    {
        const auto& record = instance_image_entry.second;

        if (record.image.id == id)
        {
            return get_image_size(record.image.image_path);
        }
    }

    throw std::runtime_error(fmt::format("Cannot determine minimum image size for id \'{}\'", id));
}
void mp::DefaultVMImageVault::clone(const std::string& dist_instance_name, const std::string& source_instance_name)
{
    const auto source_iter = instance_image_records.find(source_instance_name);

    if (source_iter == instance_image_records.end())
    {
        throw std::runtime_error(source_instance_name + " does not exist in the image records");
    }

    if (instance_image_records.find(dist_instance_name) != instance_image_records.end())
    {
        throw std::runtime_error(dist_instance_name + " already exists in the image records");
    }

    auto& dest_vault_record = instance_image_records[dist_instance_name] = instance_image_records[source_instance_name];
    dest_vault_record.image.image_path.replace(source_instance_name.c_str(), dist_instance_name.c_str());
    // change last accessed?
    persist_instance_records();
}

mp::VMImage mp::DefaultVMImageVault::download_and_prepare_source_image(
    const VMImageInfo& info, std::optional<VMImage>& existing_source_image, const QDir& image_dir,
    const FetchType& fetch_type, const PrepareAction& prepare, const ProgressMonitor& monitor)
{
    VMImage source_image;
    auto id = info.id;

    if (existing_source_image)
    {
        source_image = *existing_source_image;
    }
    else
    {
        source_image.id = id.toStdString();
        source_image.image_path = image_dir.filePath(mp::vault::filename_for(info.image_location));
        source_image.original_release = info.release_title.toStdString();
        source_image.release_date = info.version.toStdString();

        for (const auto& alias : info.aliases)
        {
            source_image.aliases.push_back(alias.toStdString());
        }
    }

    mp::vault::DeleteOnException image_file{source_image.image_path};

    try
    {
        url_downloader->download_to(info.image_location, source_image.image_path, info.size, LaunchProgress::IMAGE,
                                    monitor);

        if (info.verify)
        {
            mpl::log(mpl::Level::debug, category, fmt::format("Verifying hash \"{}\"", id));
            monitor(LaunchProgress::VERIFY, -1);
            mp::vault::verify_image_download(source_image.image_path, id);
        }

        if (source_image.image_path.endsWith(".xz"))
        {
            source_image.image_path = mp::vault::extract_image(source_image.image_path, monitor, true);
        }

        auto prepared_image = prepare(source_image);
        remove_source_images(source_image, prepared_image);

        return prepared_image;
    }
    catch (const AbortedDownloadException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw CreateImageException(e.what());
    }
}

QString mp::DefaultVMImageVault::extract_image_from(const VMImage& source_image,
                                                    const ProgressMonitor& monitor,
                                                    const mp::Path& dest_dir)
{
    MP_UTILS.make_dir(dest_dir);
    QFileInfo file_info{source_image.image_path};
    const auto image_name = file_info.fileName().remove(".xz");
    const auto image_path = QDir(dest_dir).filePath(image_name);

    return mp::vault::extract_image(image_path, monitor);
}

mp::VMImage mp::DefaultVMImageVault::image_instance_from(const VMImage& prepared_image, const mp::Path& dest_dir)
{
    MP_UTILS.make_dir(dest_dir);

    return {mp::vault::copy(prepared_image.image_path, dest_dir),
            prepared_image.id,
            prepared_image.original_release,
            prepared_image.current_release,
            prepared_image.release_date,
            {}};
}

std::optional<QFuture<mp::VMImage>> mp::DefaultVMImageVault::get_image_future(const std::string& id)
{
    auto it = in_progress_image_fetches.find(id);
    if (it != in_progress_image_fetches.end())
    {
        return it->second;
    }

    return std::nullopt;
}

mp::VMImage mp::DefaultVMImageVault::finalize_image_records(const Query& query,
                                                            const VMImage& prepared_image,
                                                            const std::string& id,
                                                            const mp::Path& dest_dir)
{
    VMImage vm_image;

    if (!query.name.empty())
    {
        vm_image = image_instance_from(prepared_image, dest_dir);
        instance_image_records[query.name] = {vm_image, query, std::chrono::system_clock::now()};
    }

    // Do not save the instance name for prepared images
    Query prepared_query{query};
    prepared_query.name = "";
    prepared_image_records[id] = {prepared_image, prepared_query, std::chrono::system_clock::now()};

    persist_instance_records();
    persist_image_records();

    return vm_image;
}

void mp::DefaultVMImageVault::persist_instance_records()
{
    persist_records(instance_image_records, data_dir.filePath(instance_db_name));
}

void mp::DefaultVMImageVault::persist_image_records()
{
    persist_records(prepared_image_records, cache_dir.filePath(image_db_name));
}
