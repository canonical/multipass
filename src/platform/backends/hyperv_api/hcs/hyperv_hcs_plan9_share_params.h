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

#include <hyperv_api/hcs/hyperv_hcs_path.h>

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

namespace detail
{
struct HcsPlan9Base
{
    /**
     * The default port number for Plan9.
     *
     * It's different from the official default port number
     * since the host might want to run a Plan9 server itself.
     */
    static constexpr std::uint16_t default_port{55035};

    /**
     * Unique name for the share
     */
    std::string name{};

    /**
     * The name by which the guest operating system can access this share
     * via the aname parameter in the Plan9 protocol.
     */
    std::string access_name{};

    /**
     * Target port.
     */
    std::uint16_t port{default_port};
};
} // namespace detail

struct HcsRemovePlan9ShareParameters : public detail::HcsPlan9Base
{
};

struct HcsAddPlan9ShareParameters : public detail::HcsPlan9Base
{
    /**
     * Host directory to share
     */
    HcsPath host_path{};

    Plan9ShareFlags flags{Plan9ShareFlags::none};
};
} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for Plan9ShareParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HcsAddPlan9ShareParameters, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HcsAddPlan9ShareParameters& param,
                FormatContext& ctx) const -> FormatContext::iterator;
};

/**
 * Formatter type specialization for Plan9ShareParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HcsRemovePlan9ShareParameters, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HcsRemovePlan9ShareParameters& param,
                FormatContext& ctx) const -> FormatContext::iterator;
};
