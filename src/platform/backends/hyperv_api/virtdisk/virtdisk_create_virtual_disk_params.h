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

/**
 * Source disk to copy the data from. This is used for cloning an existing disk into a new disk.
 */
struct SourcePathParameters
{
    std::filesystem::path path;
};

/**
 * Parent disk information. This is used for creating a virtual disk chain to layer disks.
 */
struct ParentPathParameters
{
    std::filesystem::path path;
};

struct VirtualDiskPredecessorInfo
{
    VirtualDiskPredecessorInfo() = default;

    VirtualDiskPredecessorInfo(const SourcePathParameters& param) : predecessor{param}
    {
        if (param.path.empty())
        {
            throw std::invalid_argument{"Source disk path cannot be empty."};
        }
    }

    VirtualDiskPredecessorInfo(const ParentPathParameters& param) : predecessor{param}
    {
        if (param.path.empty())
        {
            throw std::invalid_argument{"Parent disk path cannot be empty."};
        }
    }

    const auto& get() const
    {
        return predecessor;
    }

private:
    std::variant<std::monostate, SourcePathParameters, ParentPathParameters> predecessor{
        std::monostate{}};
};

/**
 * Parameters for creating a new virtual disk drive.
 */
struct CreateVirtualDiskParameters
{
    std::uint64_t size_in_bytes{};
    std::filesystem::path path{};
    /**
     * Monostate: A new disk.
     *
     * SourcePathParameters: A new disk, data and properties cloned from the disk specified by
     * SourcePathParameters.
     *
     * ParentPathParameters: A new disk, layered onto an existing disk. The
     * existing disk can be a VHDX or AVHDX.
     */
    VirtualDiskPredecessorInfo predecessor{};
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
