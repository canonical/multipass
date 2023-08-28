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

#include "lxd_vm_image_vault.h"
#include "lxd_request.h"

#include <multipass/exceptions/aborted_download_exception.h>
#include <multipass/exceptions/image_vault_exceptions.h>
#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_access_manager.h>
#include <multipass/platform.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>

#include <shared/linux/process_factory.h>
#include <shared/qemu_img_utils/qemu_img_utils.h>

#include <yaml-cpp/yaml.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTemporaryDir>

#include <chrono>
#include <thread>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace std::literals::chrono_literals;

namespace
{
constexpr auto category = "lxd image vault";

const QHash<QString, QString> host_to_lxd_arch{{"x86_64", "x86_64"}, {"arm", "armv7l"}, {"arm64", "aarch64"},
                                               {"i386", "i686"},     {"power", "ppc"},  {"power64", "ppc64"},
                                               {"s390x", "s390x"}};

auto parse_percent_as_int(const QString& progress_string)
{
    QRegularExpression re{"rootfs:\\s(\\d{1,3})%"};

    QRegularExpressionMatch match = re.match(progress_string);

    if (match.hasMatch())
    {
        return match.captured(1).toInt();
    }

    return -1;
}

QString post_process_downloaded_image(const QString& image_path, const mp::ProgressMonitor& monitor)
{
    QString new_image_path{image_path};

    if (image_path.endsWith(".xz"))
    {
        new_image_path = mp::vault::extract_image(image_path, monitor, true);
    }

    QString original_image_path{new_image_path};
    new_image_path = mp::backend::convert_to_qcow_if_necessary(new_image_path);

    if (original_image_path != new_image_path)
    {
        mp::vault::delete_file(original_image_path);
    }

    return new_image_path;
}

QString create_metadata_tarball(const mp::VMImageInfo& info, const QTemporaryDir& lxd_import_dir)
{
    QFile metadata_yaml_file{lxd_import_dir.filePath("metadata.yaml")};
    YAML::Node metadata_node;

    metadata_node["architecture"] = host_to_lxd_arch.value(QSysInfo::currentCpuArchitecture()).toStdString();
    metadata_node["creation_date"] = QDateTime::currentSecsSinceEpoch();
    metadata_node["properties"]["description"] = info.release_title.toStdString();
    metadata_node["properties"]["os"] = info.os.toStdString();
    metadata_node["properties"]["release"] = info.release.toStdString();
    metadata_node["properties"]["version"] = info.version.toStdString();
    metadata_node["properties"]["original_hash"] = info.id.toStdString();

    YAML::Emitter emitter;
    emitter << metadata_node << YAML::Newline;

    metadata_yaml_file.open(QIODevice::WriteOnly);
    metadata_yaml_file.write(emitter.c_str());
    metadata_yaml_file.close();

    const auto metadata_tarball_path = lxd_import_dir.filePath("metadata.tar");
    auto process = MP_PROCFACTORY.create_process(
        "tar", QStringList() << "-cf" << metadata_tarball_path << "-C" << lxd_import_dir.path()
                             << QFileInfo(metadata_yaml_file.fileName()).fileName());

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
    {
        throw std::runtime_error(
            fmt::format("Failed to create LXD image import metadata tarball: {}", process->read_all_standard_error()));
    }

    return metadata_tarball_path;
}

std::vector<std::string> copy_aliases(const QStringList& aliases)
{
    std::vector<std::string> copied_aliases;

    for (const auto& alias : aliases)
    {
        copied_aliases.push_back(alias.toStdString());
    }

    return copied_aliases;
}
} // namespace

mp::LXDVMImageVault::LXDVMImageVault(std::vector<VMImageHost*> image_hosts, URLDownloader* downloader,
                                     NetworkAccessManager* manager, const QUrl& base_url, const QString& cache_dir_path,
                                     const days& days_to_expire)
    : BaseVMImageVault{image_hosts},
      url_downloader{downloader},
      manager{manager},
      base_url{base_url},
      template_path{QString("%1/%2-").arg(cache_dir_path).arg(QCoreApplication::applicationName())},
      days_to_expire{days_to_expire}
{
}

mp::VMImage mp::LXDVMImageVault::fetch_image(const FetchType& fetch_type,
                                             const Query& query,
                                             const PrepareAction& prepare,
                                             const ProgressMonitor& monitor,
                                             const bool unlock,
                                             const std::optional<std::string>& checksum,
                                             const mp::Path& /* save_dir */)
{
    // Look for an already existing instance and get its image info
    try
    {
        VMImage source_image;

        auto instance_info = lxd_request(
            manager, "GET",
            QUrl(QString("%1/virtual-machines/%2").arg(base_url.toString()).arg(QString::fromStdString(query.name))));

        auto config = instance_info["metadata"].toObject()["config"].toObject();

        if (config.contains("image.original_hash"))
        {
            source_image.id = config["image.original_hash"].toString().toStdString();
            source_image.original_release = config["image.description"].toString().toStdString();
            source_image.release_date = config["image.version"].toString().toStdString();

            return source_image;
        }

        source_image.id = config["volatile.base_image"].toString().toStdString();

        if (config.contains("image.release_title"))
        {
            source_image.original_release = config["image.release_title"].toString().toStdString();
        }
        else
        {
            Query image_query;
            image_query.release = config["image.release"].toString().toStdString();

            try
            {
                const auto info = info_for(image_query);

                source_image.original_release = info->release_title.toStdString();
            }
            catch (const std::exception&)
            {
                // do nothing
            }
        }

        return source_image;
    }
    catch (const LXDNotFoundException&)
    {
        // Instance doesn't exist, so move on
    }
    catch (const LocalSocketConnectionException& e)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("{} - returning blank image info", e.what()));
        return VMImage{};
    }
    catch (const std::exception&)
    {
        // Image doesn't exist, so move on
    }

    VMImage source_image;
    VMImageInfo info;
    QString id;

    if (query.query_type == Query::Type::Alias)
    {
        if (auto image_info = info_for(query); image_info)
            info = *image_info;
        else
            throw mp::ImageNotFoundException(query.release, query.remote_name);

        id = info.id;

        source_image.id = id.toStdString();
        source_image.original_release = info.release_title.toStdString();
        source_image.release_date = info.version.toStdString();
        source_image.aliases = copy_aliases(info.aliases);
    }
    else
    {
        QUrl image_url(QString::fromStdString(query.release));
        QDateTime last_modified;

        if (query.query_type == Query::Type::HttpDownload)
        {
            // If no checksum given, generate a sha256 hash based on the URL and use that for the id
            id = checksum
                     ? QString::fromStdString(*checksum)
                     : QString(QCryptographicHash::hash(query.release.c_str(), QCryptographicHash::Sha256).toHex());
            last_modified = url_downloader->last_modified(image_url);
        }
        else
        {
            if (!QFile::exists(image_url.path()))
                throw std::runtime_error(fmt::format("Custom image `{}` does not exist.", image_url.path()));

            source_image.image_path = image_url.path();
            id = mp::vault::compute_image_hash(source_image.image_path);
            last_modified = QDateTime::currentDateTime();
        }

        info = VMImageInfo{{},
                           {},
                           {},
                           {},
                           {},
                           true,
                           image_url.url(),
                           id,
                           {},
                           last_modified.toString(),
                           0,
                           checksum.has_value()};

        source_image.id = id.toStdString();
        source_image.release_date = last_modified.toString(Qt::ISODateWithMs).toStdString();
    }

    try
    {
        auto json_reply = lxd_request(manager, "GET", QUrl(QString("%1/images/%2").arg(base_url.toString()).arg(id)));
    }
    catch (const LXDNotFoundException&)
    {
        auto lxd_image_hash = get_lxd_image_hash_for(id);
        if (!lxd_image_hash.empty())
        {
            source_image.id = lxd_image_hash;
        }
        else if (!info.stream_location.isEmpty())
        {
            lxd_download_image(info, query, monitor);
        }
        else if (!info.image_location.isEmpty())
        {
            QString image_path;
            QTemporaryDir lxd_import_dir{template_path};

            if (query.query_type != Query::Type::LocalFile)
            {
                // TODO: Need to make this async like in DefaultVMImageVault
                image_path = lxd_import_dir.filePath(mp::vault::filename_for(info.image_location));

                url_download_image(info, image_path, monitor);
            }
            else
            {
                image_path = mp::vault::copy(source_image.image_path, lxd_import_dir.path());
            }

            image_path = post_process_downloaded_image(image_path, monitor);

            monitor(LaunchProgress::WAITING, -1);

            auto metadata_tarball_path = create_metadata_tarball(info, lxd_import_dir);

            source_image.id = lxd_import_metadata_and_image(metadata_tarball_path, image_path);
        }
        else
        {
            throw std::runtime_error(fmt::format("Unable to fetch image with hash \'{}\'", id));
        }
    }

    return source_image;
}

void mp::LXDVMImageVault::remove(const std::string& name)
{
    try
    {
        auto task_reply = lxd_request(
            manager, "DELETE", QUrl(QString("%1/virtual-machines/%2").arg(base_url.toString()).arg(name.c_str())));

        lxd_wait(manager, base_url, task_reply, 120000);
    }
    catch (const LXDNotFoundException&)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Instance \'{}\' does not exist: not removing", name));
    }
}

bool mp::LXDVMImageVault::has_record_for(const std::string& name)
{
    try
    {
        lxd_request(manager, "GET", QUrl(QString("%1/virtual-machines/%2").arg(base_url.toString()).arg(name.c_str())));

        return true;
    }
    catch (const LXDNotFoundException&)
    {
        return false;
    }
    catch (const LocalSocketConnectionException& e)
    {
        mpl::log(mpl::Level::warning, category,
                 fmt::format("{} - Unable to determine if \'{}\' exists", e.what(), name));
        // Assume instance exists until it knows for sure
        return true;
    }
}

void mp::LXDVMImageVault::prune_expired_images()
{
    auto images = retrieve_image_list();

    for (const auto image : images)
    {
        auto image_info = image.toObject();
        auto properties = image_info["properties"].toObject();

        auto last_used = std::chrono::system_clock::time_point(std::chrono::milliseconds(
            QDateTime::fromString(image_info["last_used_at"].toString(), Qt::ISODateWithMs).toMSecsSinceEpoch()));

        // If the image has been downloaded but never used, then check if we added a "last_used_at" property during
        // update
        if (last_used < std::chrono::system_clock::time_point(0ms) && properties.contains("last_used_at"))
        {
            last_used = std::chrono::system_clock::time_point(std::chrono::milliseconds(
                QDateTime::fromString(properties["last_used_at"].toString(), Qt::ISODateWithMs).toMSecsSinceEpoch()));
        }

        if (last_used + days_to_expire <= std::chrono::system_clock::now())
        {
            mpl::log(mpl::Level::info, category,
                     fmt::format("Source image \'{}\' is expired. Removing it…",
                                 image_info["properties"].toObject()["release"].toString()));

            try
            {
                lxd_request(
                    manager, "DELETE",
                    QUrl(QString("%1/images/%2").arg(base_url.toString()).arg(image_info["fingerprint"].toString())));
            }
            catch (const LXDNotFoundException&)
            {
                continue;
            }
        }
    }
}

void mp::LXDVMImageVault::update_images(const FetchType& fetch_type, const PrepareAction& prepare,
                                        const ProgressMonitor& monitor)
{
    mpl::log(mpl::Level::debug, category, "Checking for images to update…");

    auto images = retrieve_image_list();

    for (const auto image : images)
    {
        auto image_info = image.toObject();
        auto image_properties = image_info["properties"].toObject();

        if (image_properties.contains("query.release"))
        {
            auto id = image_info["fingerprint"].toString();
            Query query;
            query.release = image_properties["query.release"].toString().toStdString();
            query.remote_name = image_properties["query.remote"].toString().toStdString();

            try
            {
                auto info = info_for(query);
                if (!info)
                    throw mp::ImageNotFoundException(query.release, query.remote_name);

                if (info->id != id)
                {
                    mpl::log(mpl::Level::info, category,
                             fmt::format("Updating {} source image to latest", query.release));

                    lxd_download_image(*info, query, monitor, image_info["last_used_at"].toString());

                    lxd_request(manager, "DELETE", QUrl(QString("%1/images/%2").arg(base_url.toString()).arg(id)));
                }
            }
            catch (const LXDNotFoundException&)
            {
                continue;
            }
        }
    }
}

mp::MemorySize mp::LXDVMImageVault::minimum_image_size_for(const std::string& id)
{
    MemorySize lxd_image_size{"10G"};

    try
    {
        auto json_reply = lxd_request(
            manager, "GET", QUrl(QString("%1/images/%2").arg(base_url.toString()).arg(QString::fromStdString(id))));
        const long image_size_bytes = json_reply["metadata"].toObject()["size"].toDouble();
        const MemorySize image_size{std::to_string(image_size_bytes)};

        if (image_size > lxd_image_size)
        {
            lxd_image_size = image_size;
        }
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error(fmt::format("Cannot retrieve info for image with id \'{}\': {}", id, e.what()));
    }

    return lxd_image_size;
}

void mp::LXDVMImageVault::lxd_download_image(const VMImageInfo& info, const Query& query,
                                             const ProgressMonitor& monitor, const QString& last_used)
{
    const auto id = info.id;
    QJsonObject source_object;

    source_object.insert("type", "image");
    source_object.insert("mode", "pull");
    source_object.insert("server", info.stream_location);
    source_object.insert("protocol", "simplestreams");
    source_object.insert("image_type", "virtual-machine");
    source_object.insert("fingerprint", id);

    QJsonObject image_object{{"source", source_object}};

    auto release = QString::fromStdString(query.release);

    if (!id.startsWith(release))
    {
        QJsonObject properties_object{{"query.release", release},
                                      {"query.remote", QString::fromStdString(query.remote_name)},
                                      {"release_title", info.release_title}};

        // Need to save the original image's last_used_at as a property since there is no way to modify the
        // new image's last_used_at field.
        if (!last_used.isEmpty())
        {
            properties_object.insert("last_used_at", last_used);
        }

        image_object.insert("properties", properties_object);
    }

    auto json_reply = lxd_request(manager, "POST", QUrl(QString("%1/images").arg(base_url.toString())), image_object);

    poll_download_operation(json_reply, monitor);
}

void mp::LXDVMImageVault::url_download_image(const VMImageInfo& info, const QString& image_path,
                                             const ProgressMonitor& monitor)
{
    mp::vault::DeleteOnException image_file{image_path};

    url_downloader->download_to(info.image_location, image_path, info.size, LaunchProgress::IMAGE, monitor);

    if (info.verify)
    {
        monitor(LaunchProgress::VERIFY, -1);
        mp::vault::verify_image_download(image_path, info.id);
    }
}

void mp::LXDVMImageVault::poll_download_operation(const QJsonObject& json_reply, const ProgressMonitor& monitor)
{
    if (json_reply["metadata"].toObject()["class"] == QStringLiteral("task") &&
        json_reply["status_code"].toInt(-1) == 100)
    {
        QUrl task_url(QString("%1/operations/%2")
                          .arg(base_url.toString())
                          .arg(json_reply["metadata"].toObject()["id"].toString()));

        // Instead of polling, need to use websockets to get events
        auto last_download_progress = -2;
        while (true)
        {
            try
            {
                auto task_reply = mp::lxd_request(manager, "GET", task_url);

                if (task_reply["error_code"].toInt(-1) != 0)
                {
                    mpl::log(mpl::Level::error, category, task_reply["error"].toString().toStdString());
                    break;
                }

                auto status_code = task_reply["metadata"].toObject()["status_code"].toInt(-1);
                if (status_code == 200)
                {
                    break;
                }
                else
                {
                    auto download_progress = parse_percent_as_int(
                        task_reply["metadata"].toObject()["metadata"].toObject()["download_progress"].toString());

                    if (last_download_progress != download_progress &&
                        !monitor(LaunchProgress::IMAGE, download_progress))
                    {
                        mp::lxd_request(manager, "DELETE", task_url);
                        throw mp::AbortedDownloadException{"Download aborted"};
                    }

                    last_download_progress = download_progress;

                    std::this_thread::sleep_for(1s);
                }
            }
            // Implies the task is finished
            catch (const LXDNotFoundException&)
            {
                break;
            }
        }
    }
}

std::string mp::LXDVMImageVault::lxd_import_metadata_and_image(const QString& metadata_path, const QString& image_path)
{
    QHttpMultiPart lxd_multipart{QHttpMultiPart::FormDataType};
    QFileInfo metadata_info{metadata_path}, image_info{image_path};

    QHttpPart metadata_part;
    metadata_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    metadata_part.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"metadata\"; filename=\"%1\"").arg(metadata_info.fileName())));
    QFile* metadata_file = new QFile(metadata_path);
    metadata_file->open(QIODevice::ReadOnly);
    metadata_part.setBodyDevice(metadata_file);
    metadata_file->setParent(&lxd_multipart);

    QHttpPart image_part;
    image_part.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
    image_part.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QVariant(QString("form-data; name=\"rootfs.img\"; filename=\"%1\"").arg(image_info.fileName())));
    QFile* image_file = new QFile(image_path);
    image_file->open(QIODevice::ReadOnly);
    image_part.setBodyDevice(image_file);
    image_file->setParent(&lxd_multipart);

    lxd_multipart.append(metadata_part);
    lxd_multipart.append(image_part);

    auto json_reply = lxd_request(manager, "POST", QUrl(QString("%1/images").arg(base_url.toString())), lxd_multipart);

    auto task_reply = lxd_wait(manager, base_url, json_reply, 300000);

    return task_reply["metadata"].toObject()["metadata"].toObject()["fingerprint"].toString().toStdString();
}

std::string mp::LXDVMImageVault::get_lxd_image_hash_for(const QString& id)
{
    auto images = retrieve_image_list();

    for (const auto image : images)
    {
        auto image_info = image.toObject();
        auto properties = image_info["properties"].toObject();

        if (properties.contains("original_hash"))
        {
            auto original_hash = properties["original_hash"].toString();
            if (original_hash == id)
            {
                return image_info["fingerprint"].toString().toStdString();
            }
        }
    }

    return {};
}

QJsonArray mp::LXDVMImageVault::retrieve_image_list()
{
    QJsonArray image_list;

    try
    {
        auto json_reply = lxd_request(manager, "GET", QUrl(QString("%1/images?recursion=1").arg(base_url.toString())));

        image_list = json_reply["metadata"].toArray();
    }
    catch (const LXDNotFoundException&)
    {
        // ignore exception
    }
    catch (const LocalSocketConnectionException& e)
    {
        mpl::log(mpl::Level::warning, category, e.what());
    }

    return image_list;
}
