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

#ifndef MULTIPASS_HYPERV_API_VIRTDISK_CREATE_VIRTUAL_DISK_PARAMETERS_H
#define MULTIPASS_HYPERV_API_VIRTDISK_CREATE_VIRTUAL_DISK_PARAMETERS_H

#include <filesystem>
#include <fmt/format.h>


namespace multipass::hyperv::virtdisk
{

/**
 * Parameters for creating a new virtual disk drive.
 */
struct CreateVirtualDiskParameters
{
    std::uint64_t size_in_bytes{};
    std::filesystem::path path{};
};

} // namespace multipass::hyperv::virtdisk

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::virtdisk::CreateVirtualDiskParameters, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::virtdisk::CreateVirtualDiskParameters& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "Size (in bytes): ({}) | Path: ({}) ", params.size_in_bytes, params.path.string());
    }
};

#endif
