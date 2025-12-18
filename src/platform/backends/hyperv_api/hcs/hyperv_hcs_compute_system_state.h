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

#include <multipass/utils/static_bimap.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

namespace multipass::hyperv::hcs
{

/**
 * Enum values representing a compute system's possible state
 *
 * @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#State
 */
enum class ComputeSystemState : std::uint8_t
{
    created,
    running,
    paused,
    stopped,
    saved_as_template,
    unknown,
};

namespace detail
{
[[nodiscard]] inline const auto& compute_system_state_map()
{
    static const static_bi_map<std::string, ComputeSystemState> state_map{
        {"created", ComputeSystemState::created},
        {"running", ComputeSystemState::running},
        {"paused", ComputeSystemState::paused},
        {"stopped", ComputeSystemState::stopped},
        {"savedastemplate", ComputeSystemState::saved_as_template},
        {"unknown", ComputeSystemState::unknown},
    };
    return state_map;
}
} // namespace detail

/**
 * Translate host compute system state string to enum
 *
 * @param str
 * @return ComputeSystemState
 */
[[nodiscard]] inline std::optional<ComputeSystemState> compute_system_state_from_string(
    std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    const auto& map = detail::compute_system_state_map().left;

    if (const auto itr = map.find(str); map.end() != itr)
        return itr->second;

    return std::nullopt;
}

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::ComputeSystemState, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(multipass::hyperv::hcs::ComputeSystemState state, FormatContext& ctx) const
    {
        std::string_view v = "(undefined)";
        const auto& map = multipass::hyperv::hcs::detail::compute_system_state_map().right;

        if (const auto itr = map.find(state); map.end() != itr)
            v = itr->second;

        return fmt::format_to(ctx.out(), "{}", v);
    }
};
