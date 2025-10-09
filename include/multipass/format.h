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

#pragma once

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <QByteArray>
#include <QProcess>
#include <QString>

#include <multipass/rpc/multipass.grpc.pb.h>

namespace fmt
{
template <>
struct formatter<QByteArray>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const QByteArray& a, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", a.toStdString()); // TODO: remove the copy?
    }
};

template <>
struct formatter<QString>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const QString& a, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", a.toStdString()); // TODO: remove the copy?
    }
};

template <>
struct formatter<QProcess::ExitStatus>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const QProcess::ExitStatus& exit_status, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", static_cast<int>(exit_status));
    }
};

template <>
struct formatter<multipass::MountInfo_MountPaths> : formatter<string_view>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::MountInfo_MountPaths& mount_path, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{} => {}", mount_path.source_path(), mount_path.target_path());
    }
};

template <>
struct formatter<multipass::MountInfo> : formatter<string_view>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::MountInfo& mount_info, FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", fmt::join(mount_info.mount_paths(), ";"));
    }
};

} // namespace fmt
