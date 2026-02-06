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
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

#include <QJsonArray>
#include <QJsonDocument>

#include <boost/algorithm/string/replace.hpp>

#include <sstream>
#include <stack>
#include <stdexcept>
#include <variant>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
using JsonObjectIter = std::counted_iterator<boost::json::object::const_iterator>;
using JsonArrayIter = std::counted_iterator<boost::json::array::const_iterator>;
using JsonIter = std::variant<JsonObjectIter, JsonArrayIter>;

JsonObjectIter counted_iter(const boost::json::object& object)
{
    return {object.begin(), static_cast<std::ptrdiff_t>(object.size())};
}

JsonArrayIter counted_iter(const boost::json::array& array)
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
    // Add a comma if there are any more elements after the current one.
    if (iter.count())
        os.put(',');
}

void maybe_put_comma(std::ostream& os, const JsonIter& iter)
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

mp::JsonUtils::JsonUtils(const Singleton<JsonUtils>::PrivatePass& pass) noexcept
    : Singleton<JsonUtils>{pass}
{
}

QJsonObject mp::JsonUtils::read_object_from_file(const std::filesystem::path& file_path) const
{
    const auto file = MP_FILEOPS.open_read(file_path);
    file->exceptions(std::ifstream::failbit | std::ifstream::badbit);
    const auto data =
        QString::fromStdString(std::string{std::istreambuf_iterator{*file}, {}}).toUtf8();
    return QJsonDocument::fromJson(data).object();
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

boost::json::object mp::update_unique_identifiers_of_metadata(const boost::json::object& metadata,
                                                              const multipass::VMSpecs& src_specs,
                                                              const multipass::VMSpecs& dest_specs,
                                                              const std::string& src_vm_name,
                                                              const std::string& dest_vm_name)
{
    assert(src_specs.extra_interfaces.size() == dest_specs.extra_interfaces.size());

    boost::json::object result_metadata = metadata;
    for (auto& item : result_metadata.at("arguments").as_array())
    {
        auto str = value_to<std::string>(item);
        boost::replace_all(str, src_specs.default_mac_address, dest_specs.default_mac_address);
        for (size_t i = 0; i < src_specs.extra_interfaces.size(); ++i)
        {
            const std::string& src_mac = src_specs.extra_interfaces[i].mac_address;
            if (!src_mac.empty())
            {
                const std::string& dest_mac = dest_specs.extra_interfaces[i].mac_address;
                boost::replace_all(str, src_mac, dest_mac);
            }
        }
        // string replacement is "instances/<src_name>"->"instances/<dest_name>" instead of
        // "<src_name>"->"<dest_name>", because the second one might match other substrings of
        // the metadata.
        boost::replace_all(str, "instances/" + src_vm_name, "instances/" + dest_vm_name);
        item = boost::json::string(str);
    }

    return result_metadata;
}

// Pretty print a JSON value non-recursively. We need to avoid recursion since we serialize
// untrusted user data and want to prevent stack overflows.
//
// In this implementation our general strategy is to maintain a `std::stack` of counted iterators
// representing our progress through each nested container (arrays or objects). While processing a
// particular container, we can immediately print any scalars we encounter, but if we hit another
// container, we push its iterator onto the stack and break out of our inner loop to start
// processing that new container.
void mp::pretty_print(std::ostream& os,
                      const boost::json::value& value,
                      const PrettyPrintOptions& opts)
{
    std::stack<JsonIter> stack;

    // Populate our stack with the top-level sequence (or just return immediately if `value` is a
    // scalar).
    if (auto&& obj = value.if_object())
    {
        stack.push(counted_iter(*obj));
        os.put('{');
    }
    else if (auto&& arr = value.if_array())
    {
        stack.push(counted_iter(*arr));
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
        if (std::holds_alternative<JsonObjectIter>(stack.top()))
        {
            auto& i = std::get<JsonObjectIter>(stack.top());
            while (i != std::default_sentinel)
            {
                indent(os, opts.indent * stack.size());
                os << boost::json::serialize(i->key()) << ": ";
                if (auto&& obj = i->value().if_object())
                {
                    ++i;
                    stack.push(counted_iter(*obj));
                    os.put('{');
                    goto outer;
                }
                else if (auto&& arr = i->value().if_array())
                {
                    ++i;
                    stack.push(counted_iter(*arr));
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
            stack.pop();

            indent(os, opts.indent * stack.size());
            os.put('}');
            if (!stack.empty())
                maybe_put_comma(os, stack.top());
        }
        else // array
        {
            auto& i = std::get<JsonArrayIter>(stack.top());
            while (i != std::default_sentinel)
            {
                indent(os, opts.indent * stack.size());
                if (auto&& obj = i->if_object())
                {
                    ++i;
                    stack.push(counted_iter(*obj));
                    os.put('{');
                    goto outer;
                }
                else if (auto&& arr = i->if_array())
                {
                    ++i;
                    stack.push(counted_iter(*arr));
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
            stack.pop();

            indent(os, opts.indent * stack.size());
            os.put(']');
            if (!stack.empty())
                maybe_put_comma(os, stack.top());
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

    assert(false && "unsupported type");
    std::abort();
}

void tag_invoke(const boost::json::value_from_tag&,
                boost::json::value& json,
                const QStringList& list)
{
    auto& arr = json.emplace_array();
    for (const auto& i : list)
        arr.emplace_back(i.toStdString());
}

QStringList tag_invoke(const boost::json::value_to_tag<QStringList>&,
                       const boost::json::value& json)
{
    QStringList result;
    for (const auto& i : json.as_array())
        result.emplace_back(value_to<QString>(i));
    return result;
}
