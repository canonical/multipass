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

#ifndef MULTIPASS_HYPERV_API_VIRTDISK_DISK_INFO_H
#define MULTIPASS_HYPERV_API_VIRTDISK_DISK_INFO_H

#include <fmt/format.h>
#include <fmt/std.h>

#include <cstdint>
#include <optional>

namespace multipass::hyperv::virtdisk
{

struct VirtualDiskInfo
{

    struct size_info
    {
        std::uint64_t virtual_;
        std::uint64_t physical;
        std::uint64_t block;
        std::uint64_t sector;
    };
    std::optional<size_info> size;
    std::optional<std::uint64_t> smallest_safe_virtual_size;
    std::optional<std::string> virtual_storage_type;
    std::optional<std::string> provider_subtype;
};

} // namespace multipass::hyperv::virtdisk

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::virtdisk::VirtualDiskInfo::size_info, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::virtdisk::VirtualDiskInfo::size_info& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Virtual: ({}) | Physical: ({}) | Block: ({}) | Sector: ({})",
                         params.virtual_,
                         params.physical,
                         params.block,
                         params.sector);
    }
};

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::virtdisk::VirtualDiskInfo, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::virtdisk::VirtualDiskInfo& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Storage type: {} | Size: {} | Smallest safe size: {} | Provider subtype: {}",
                         params.virtual_storage_type,
                         params.size,
                         params.smallest_safe_virtual_size,
                         params.provider_subtype);
    }
};

#endif