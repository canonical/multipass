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

#ifndef MULTIPASS_HYPERV_API_HCS_PLAN9_SHARE_PARAMS_H
#define MULTIPASS_HYPERV_API_HCS_PLAN9_SHARE_PARAMS_H

#include <filesystem>
#include <fmt/format.h>
#include <string>

namespace multipass::hyperv::hcs
{

enum class Plan9ShareFlags : std::uint32_t
{
    none = 0,
    read_only = 0x00000001,
    linux_metadata = 0x00000004,
    case_sensitive = 0x00000008
};

/**
 * Parameters for creating a Plan9 share.
 */
struct Plan9ShareParameters
{
    /**
     * The default port number for Plan9.
     *
     * It's different from the official default port number
     * since the host might want to run a Plan9 server itself.
     */
    static inline constexpr std::uint16_t default_port{55035};

    /**
     * Unique name for the share
     */
    std::string name{};

    /**
     * The name by which the guest operation system can access this share
     * via the aname parameter in the Plan9 protocol.
     */
    std::string access_name{};

    /**
     * Host directory to share
     */
    std::filesystem::path host_path{};

    /**
     * Target path.
     */
    std::uint16_t port{default_port};

    /**
     * ReadOnly      0x00000001
     * LinuxMetadata 0x00000004
     * CaseSensitive 0x00000008
     */
    Plan9ShareFlags flags{Plan9ShareFlags::none};
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::Plan9ShareParameters, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::Plan9ShareParameters& params, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Share name: ({}) | Access name: ({}) | Host path: ({}) | Port: ({}) ",
                         params.name,
                         params.access_name,
                         params.host_path,
                         params.port,
                         fmt::underlying(params.flags));
    }
};

#endif // MULTIPASS_HYPERV_API_HCS_ADD_9P_SHARE_PARAMS_H
