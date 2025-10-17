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

#include <filesystem>
#include <variant>

#include <fmt/format.h>

namespace multipass::hyperv::virtdisk
{

struct SourcePathParameters
{
    std::filesystem::path path;
};
struct ParentPathParameters
{
    std::filesystem::path path;
};

/**
 * Parameters for creating a new virtual disk drive.
 */
struct CreateVirtualDiskParameters
{
    std::uint64_t size_in_bytes{};
    std::filesystem::path path{};
    std::variant<std::monostate, SourcePathParameters, ParentPathParameters> predecessor{};
};

} // namespace multipass::hyperv::virtdisk

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::virtdisk::CreateVirtualDiskParameters, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::virtdisk::CreateVirtualDiskParameters& params,
                FormatContext& ctx) const
    {
        return fmt::format_to(ctx.out(),
                              "Size (in bytes): ({}) | Path: ({}) ",
                              params.size_in_bytes,
                              params.path.string());
    }
};
