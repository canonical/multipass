/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_FORMAT_UTILS_H
#define MULTIPASS_FORMAT_UTILS_H

#include <multipass/constants.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings.h>

#include <fmt/format.h>

#include <algorithm>
#include <string>

namespace multipass
{
class Formatter;

namespace format
{
std::string state_string_for(const InstanceState& state);
std::string image_string_for(const multipass::FindReply_AliasInfo& alias);
Formatter* formatter_for(const std::string& format);

template <typename Instances>
Instances sorted(const Instances& instances);

void filter_aliases(google::protobuf::RepeatedPtrField<multipass::FindReply_AliasInfo>& aliases);
} // namespace format
}

template <typename Instances>
Instances multipass::format::sorted(const Instances& instances)
{
    if (instances.empty())
        return instances;

    auto ret = instances;
    const auto petenv_name = Settings::instance().get(petenv_key).toStdString();
    std::partial_sort(std::begin(ret), std::next(std::begin(ret)), std::end(ret),
                      [&petenv_name](const auto& a, const auto& /*unused*/) { return a.name() == petenv_name; });

    return ret;
}

namespace fmt
{
template <>
struct formatter<multipass::FindReply_AliasInfo>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::FindReply_AliasInfo& a, FormatContext& ctx)
    {
        return format_to(ctx.begin(), "{}", a.alias());
    }
};
} // namespace fmt

#endif // MULTIPASS_FORMAT_UTILS_H
