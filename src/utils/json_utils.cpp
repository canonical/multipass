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

#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

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
