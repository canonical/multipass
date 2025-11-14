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

#include <iostream>

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
#include <sstream>
#include <stdexcept>
#include <thread>
#include <variant>

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
    QLockFile lock(fi.filePath() + ".lock");

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

namespace
{
using object_iter_t = std::counted_iterator<boost::json::object::const_iterator>;
using array_iter_t = std::counted_iterator<boost::json::array::const_iterator>;
using some_iter_t = std::variant<object_iter_t, array_iter_t>;

object_iter_t counted_iter(const boost::json::object& object)
{
    return {object.begin(), static_cast<std::ptrdiff_t>(object.size())};
}

array_iter_t counted_iter(const boost::json::array& array)
{
    return {array.begin(), static_cast<std::ptrdiff_t>(array.size())};
}

void indent(std::ostream& os, std::size_t width)
{
    for (std::size_t i = 0; i != width; i++)
        os.put(' ');
}

template <typename T>
void maybe_put_comma(std::ostream& os, const std::counted_iterator<T>& iter)
{
    if (iter.count())
        os.put(',');
}

void maybe_put_comma(std::ostream& os, const some_iter_t& iter)
{
    std::visit([&os](auto&& arg) { maybe_put_comma(os, arg); }, iter);
}

void pretty_print_scalar(std::ostream& os, const boost::json::value& value)
{
    assert(!value.is_object() && !value.is_array());
    // Boost.JSON always prints doubles in scientific notation, but that's not pretty!
    if (value.kind() == boost::json::kind::double_)
        fmt::print(os, "{}", value.as_double());
    else
        os << serialize(value);
}
} // namespace

void mp::pretty_print(std::ostream& os,
                      const boost::json::value& value,
                      const PrettyPrintOptions& opts)
{
    std::vector<some_iter_t> stack;

    // Populate our stack with the top-level sequence (or just return immediately if `value` is a
    // scalar).
    if (auto&& obj = value.if_object())
    {
        stack.push_back(counted_iter(*obj));
        os.put('{');
    }
    else if (auto&& arr = value.if_array())
    {
        stack.push_back(counted_iter(*arr));
        os.put('[');
    }
    else
    {
        pretty_print_scalar(os, value);
        if (opts.trailing_newline)
            os.put('\n');
        return;
    }

    while (!stack.empty())
    {
    outer:
        os.put('\n');
        if (std::holds_alternative<object_iter_t>(stack.back()))
        {
            auto& i = std::get<object_iter_t>(stack.back());
            while (i != std::default_sentinel)
            {
                indent(os, opts.indent * stack.size());
                os << boost::json::serialize(i->key()) << ": ";
                if (auto&& obj = i->value().if_object())
                {
                    ++i;
                    stack.push_back(counted_iter(*obj));
                    os.put('{');
                    goto outer;
                }
                else if (auto&& arr = i->value().if_array())
                {
                    ++i;
                    stack.push_back(counted_iter(*arr));
                    os.put('[');
                    goto outer;
                }
                else
                {
                    pretty_print_scalar(os, i->value());
                }

                ++i;
                maybe_put_comma(os, i);
                os.put('\n');
            }
            stack.pop_back();

            indent(os, opts.indent * stack.size());
            os.put('}');
            if (!stack.empty())
                maybe_put_comma(os, stack.back());
        }
        else // array
        {
            auto& i = std::get<array_iter_t>(stack.back());
            while (i != std::default_sentinel)
            {
                indent(os, opts.indent * stack.size());
                if (auto&& obj = i->if_object())
                {
                    ++i;
                    stack.push_back(counted_iter(*obj));
                    os.put('{');
                    goto outer;
                }
                else if (auto&& arr = i->if_array())
                {
                    ++i;
                    stack.push_back(counted_iter(*arr));
                    os.put('[');
                    goto outer;
                }
                else
                {
                    pretty_print_scalar(os, *i);
                }

                ++i;
                maybe_put_comma(os, i);
                os.put('\n');
            }
            stack.pop_back();

            indent(os, opts.indent * stack.size());
            os.put(']');
            if (!stack.empty())
                maybe_put_comma(os, stack.back());
        }
    }

    if (opts.trailing_newline)
        os.put('\n');
}

std::string mp::pretty_print(const boost::json::value& value, const PrettyPrintOptions& opts)
{
    std::ostringstream os;
    pretty_print(os, value, opts);
    return os.str();
}

boost::json::value mp::qjson_to_boost_json(const QJsonValue& value)
{
    QJsonDocument doc;
    switch (value.type())
    {
    case QJsonValue::Array:
        doc = QJsonDocument{value.toArray()};
        break;
    case QJsonValue::Object:
        doc = QJsonDocument{value.toObject()};
        break;
    default:
        assert(false && "unsupported type");
    }
    return boost::json::parse(std::string_view(doc.toJson()));
}

QJsonValue mp::boost_json_to_qjson(const boost::json::value& value)
{
    auto json_data = serialize(value);
    auto doc = QJsonDocument::fromJson(
        QByteArray{json_data.data(), static_cast<qsizetype>(json_data.size())});
    if (doc.isArray())
        return doc.array();
    else if (doc.isObject())
        return doc.object();
    else
        assert(false && "unsupported type");
}

boost::json::array mp::string_list_to_boost_json(const QStringList& list)
{
    boost::json::array result;
    for (const auto& i : list)
        result.emplace_back(i.toStdString());
    return result;
}

QStringList mp::boost_json_to_string_list(const boost::json::array& list)
{
    QStringList result;
    for (const auto& i : list)
        result.emplace_back(QString::fromStdString(value_to<std::string>(i)));
    return result;
}
