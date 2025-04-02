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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>

#include <chrono>
#include <random>
#include <stdexcept>
#include <thread>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
constexpr static auto kLogCategory = "JsonUtils";
}

mp::JsonUtils::JsonUtils(const Singleton<JsonUtils>::PrivatePass& pass) noexcept
    : Singleton<JsonUtils>{pass}
{
}

void mp::JsonUtils::write_json(const QJsonObject& root, QString file_name) const
{
    constexpr static auto kStaleLockTime = std::chrono::seconds{10};
    constexpr static auto kLockAcquireTimeout = std::chrono::seconds{10};
    const QFileInfo fi{file_name};

    const auto dir = fi.absoluteDir();
    if (!MP_FILEOPS.mkpath(fi.absoluteDir(), "."))
        throw std::runtime_error(fmt::format("Could not create path '{}'", dir.absolutePath()));

    // Interprocess lock file to ensure that we can synchronize the request from
    // both the daemon and the client.
    QLockFile lock(fi.filePath() + u".lock"_qs);

    // Make the lock file stale after a while to avoid deadlocking
    // on process crashes, etc.
    MP_FILEOPS.setStaleLockTime(lock, kStaleLockTime);

    // Acquire lock file before attempting to write.
    if (!MP_FILEOPS.tryLock(lock, kLockAcquireTimeout))
    { // wait up to 10s
        throw std::runtime_error(fmt::format("Could not acquire lock for '{}'", file_name));
    }

    constexpr static auto max_attempts = 10;

    // The retry logic is here because the destination file might be locked for any reason
    // (e.g. OS background indexing) so we will retry writing it until it's successful
    // or the attempts are exhausted.
    for (auto attempt = 1; attempt <= max_attempts; attempt++)
    {
        QSaveFile db_file{file_name};
        if (!MP_FILEOPS.open(db_file, QIODevice::WriteOnly))
            throw std::runtime_error{
                fmt::format("Could not open transactional file for writing; filename: {}",
                            file_name)};

        if (MP_FILEOPS.write(db_file, QJsonDocument{root}.toJson()) == -1)
            throw std::runtime_error{
                fmt::format("Could not write json to transactional file; filename: {}; error: {}",
                            file_name,
                            db_file.errorString())};

        if (!MP_FILEOPS.commit(db_file))
        {
            auto get_jitter_amount = [] {
                constexpr static auto kMaxJitter = 25;
                thread_local std::mt19937 rng{std::random_device{}()};
                thread_local std::uniform_int_distribution<int> jit(0, kMaxJitter);
                return jit(rng);
            };

            // Delay with jitter + backoff. A typical series produced
            // by this formula would look like as follows:
            // [2, 14,23,60,90,168,216,213,218,218]
            // [14,20,30,40,98,174,221,208,206,214]
            const auto delay = std::chrono::milliseconds(std::min(200, 10 * (1 << (attempt - 1))) +
                                                         get_jitter_amount());
            mpl::warn(
                kLogCategory,
                "Failed to write `{}` in attempt #{} (reason: {}), will retry after {} ms delay.",
                file_name,
                attempt,
                db_file.errorString(),
                delay);

            std::this_thread::sleep_for(delay);
        }
        else
        {
            // Saved successfully
            mpl::debug(kLogCategory,
                       "Saved file `{}` successfully in attempt #{}",
                       file_name,
                       attempt);
            return;
        }
    }

    throw std::runtime_error{
        fmt::format("Could not commit transactional file; filename: {}", file_name)};
}

QJsonObject mp::JsonUtils::read_object_from_file(const std::filesystem::path& file_path) const
try
{
    const auto file = MP_FILEOPS.open_read(file_path);
    const auto data = QString::fromStdString(std::string{std::istreambuf_iterator{*file}, {}}).toUtf8();
    return QJsonDocument::fromJson(data).object();
}
catch (const std::exception& e)
{
    throw mp::FormattedExceptionBase{"failed to read JSON from file '{}': {}", file_path, e.what()};
}

std::string mp::JsonUtils::json_to_string(const QJsonObject& root) const
{
    // The function name toJson() is shockingly wrong, for it converts an actual JsonDocument to a
    // QByteArray.
    return QJsonDocument(root).toJson().toStdString();
}

QJsonValue mp::JsonUtils::update_cloud_init_instance_id(const QJsonValue& id,
                                                        const std::string& src_vm_name,
                                                        const std::string& dest_vm_name) const
{
    std::string id_str = id.toString().toStdString();
    assert(id_str.size() >= src_vm_name.size());

    return QJsonValue{QString::fromStdString(id_str.replace(0, src_vm_name.size(), dest_vm_name))};
}

QJsonValue mp::JsonUtils::update_unique_identifiers_of_metadata(
    const QJsonValue& metadata,
    const multipass::VMSpecs& src_specs,
    const multipass::VMSpecs& dest_specs,
    const std::string& src_vm_name,
    const std::string& dest_vm_name) const
{
    assert(src_specs.extra_interfaces.size() == dest_specs.extra_interfaces.size());

    QJsonObject result_metadata_object = metadata.toObject();
    QJsonValueRef arguments = result_metadata_object["arguments"];
    QJsonArray json_array = arguments.toArray();
    for (QJsonValueRef item : json_array)
    {
        QString str = item.toString();

        str.replace(src_specs.default_mac_address.c_str(), dest_specs.default_mac_address.c_str());
        for (size_t i = 0; i < src_specs.extra_interfaces.size(); ++i)
        {
            const std::string& src_mac = src_specs.extra_interfaces[i].mac_address;
            if (!src_mac.empty())
            {
                const std::string& dest_mac = dest_specs.extra_interfaces[i].mac_address;
                str.replace(src_mac.c_str(), dest_mac.c_str());
            }
        }
        // string replacement is "instances/<src_name>"->"instances/<dest_name>" instead of
        // "<src_name>"->"<dest_name>", because the second one might match other substrings of the
        // metadata.
        str.replace("instances/" + QString{src_vm_name.c_str()},
                    "instances/" + QString{dest_vm_name.c_str()});
        item = str;
    }
    arguments = json_array;

    return QJsonValue{result_metadata_object};
}

QJsonArray mp::JsonUtils::extra_interfaces_to_json_array(
    const std::vector<mp::NetworkInterface>& extra_interfaces) const
{
    QJsonArray json;

    for (const auto& interface : extra_interfaces)
    {
        QJsonObject entry;
        entry.insert("id", QString::fromStdString(interface.id));
        entry.insert("mac_address", QString::fromStdString(interface.mac_address));
        entry.insert("auto_mode", interface.auto_mode);
        json.append(entry);
    }

    return json;
}

std::optional<std::vector<mp::NetworkInterface>> mp::JsonUtils::read_extra_interfaces(
    const QJsonObject& record) const
{
    if (record.contains("extra_interfaces"))
    {
        std::vector<mp::NetworkInterface> extra_interfaces;

        for (QJsonValueRef entry : record["extra_interfaces"].toArray())
        {
            auto id = entry.toObject()["id"].toString().toStdString();
            auto mac_address = entry.toObject()["mac_address"].toString().toStdString();
            if (!mpu::valid_mac_address(mac_address))
            {
                throw std::runtime_error(fmt::format("Invalid MAC address {}", mac_address));
            }
            auto auto_mode = entry.toObject()["auto_mode"].toBool();
            extra_interfaces.push_back(mp::NetworkInterface{id, mac_address, auto_mode});
        }

        return extra_interfaces;
    }

    return std::nullopt;
}
