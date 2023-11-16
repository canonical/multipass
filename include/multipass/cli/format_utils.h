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

#ifndef MULTIPASS_FORMAT_UTILS_H
#define MULTIPASS_FORMAT_UTILS_H

#include <multipass/constants.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings/settings.h>

#include <fmt/format.h>
#include <google/protobuf/util/time_util.h>

#include <algorithm>
#include <string>

#define MP_FORMAT_UTILS multipass::FormatUtils::instance()

namespace multipass
{
class Formatter;

namespace format
{
static constexpr int col_buffer = 3;

std::string status_string_for(const InstanceStatus& status);
std::string image_string_for(const multipass::FindReply_AliasInfo& alias);
Formatter* formatter_for(const std::string& format);

template <typename Container>
Container sorted(const Container& items);

void filter_aliases(google::protobuf::RepeatedPtrField<multipass::FindReply_AliasInfo>& aliases);

// Computes the column width needed to display all the elements of a range [begin, end). get_width is a function
// which takes as input the element in the range and returns its width in columns.
static constexpr auto column_width =
    [](const auto begin, const auto end, const auto get_width, int header_width, int minimum_width = 0) {
        if (0 == std::distance(begin, end))
            return std::max({header_width + col_buffer, minimum_width});

        auto max_width = std::max_element(begin, end, [&get_width](auto& lhs, auto& rhs) {
            return get_width(lhs) < get_width(rhs);
        });
        return std::max({get_width(*max_width) + col_buffer, header_width + col_buffer, minimum_width});
    };
} // namespace format

class FormatUtils : public Singleton<FormatUtils>
{
public:
    FormatUtils(const Singleton<FormatUtils>::PrivatePass&) noexcept;

    virtual std::string convert_to_user_locale(const google::protobuf::Timestamp& timestamp) const;
};
} // namespace multipass

template <typename Container>
Container multipass::format::sorted(const Container& items)
{
    if (items.empty())
        return items;

    auto ret = items;
    const auto petenv_name = MP_SETTINGS.get(petenv_key).toStdString();
    std::sort(std::begin(ret), std::end(ret), [&petenv_name](const auto& a, const auto& b) {
        using T = std::decay_t<decltype(a)>;
        using google::protobuf::util::TimeUtil;

        // Put instances first when sorting info reply
        if constexpr (std::is_same_v<T, multipass::DetailedInfoItem>)
        {
            if (a.has_instance_info() && b.has_snapshot_info())
                return true;
            else if (a.has_snapshot_info() && b.has_instance_info())
                return false;
        }

        // Put petenv related entries first
        if (a.name() == petenv_name && b.name() != petenv_name)
            return true;
        else if (b.name() == petenv_name && a.name() != petenv_name)
            return false;
        else
        {
            // Sort by timestamp when names are the same for snapshots
            if constexpr (std::is_same_v<T, multipass::DetailedInfoItem>)
            {
                if (a.has_snapshot_info() && a.name() == b.name())
                    return TimeUtil::TimestampToNanoseconds(a.snapshot_info().fundamentals().creation_timestamp()) <
                           TimeUtil::TimestampToNanoseconds(b.snapshot_info().fundamentals().creation_timestamp());
            }
            else if constexpr (std::is_same_v<T, multipass::ListVMSnapshot>)
            {
                if (a.name() == b.name())
                    return TimeUtil::TimestampToNanoseconds(a.fundamentals().creation_timestamp()) <
                           TimeUtil::TimestampToNanoseconds(b.fundamentals().creation_timestamp());
            }

            // Lastly, sort by name
            return a.name() < b.name();
        }
    });

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
    auto format(const multipass::FindReply_AliasInfo& a, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", a.alias());
    }
};
} // namespace fmt

#endif // MULTIPASS_FORMAT_UTILS_H
