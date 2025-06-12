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

#ifndef MULTIPASS_HYPERV_API_HCS_CREATE_COMPUTE_SYSTEM_STATE_H
#define MULTIPASS_HYPERV_API_HCS_CREATE_COMPUTE_SYSTEM_STATE_H

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

/**
 * Translate host compute system state string to enum
 *
 * @param str
 * @return ComputeSystemState
 */
inline std::optional<ComputeSystemState> compute_system_state_from_string(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });

    static const std::unordered_map<std::string, ComputeSystemState> translation_map{
        {"created", ComputeSystemState::created},
        {"running", ComputeSystemState::running},
        {"paused", ComputeSystemState::paused},
        {"stopped", ComputeSystemState::stopped},
        {"savedastemplate", ComputeSystemState::saved_as_template},
        {"unknown", ComputeSystemState::unknown},
    };

    if (const auto itr = translation_map.find(str); translation_map.end() != itr)
        return itr->second;

    return std::nullopt;
}

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::ComputeSystemState, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(multipass::hyperv::hcs::ComputeSystemState state, FormatContext& ctx) const
    {
        std::string_view v = "(undefined)";
        switch (state)
        {
        case multipass::hyperv::hcs::ComputeSystemState::created:
            v = "created";
            break;
        case multipass::hyperv::hcs::ComputeSystemState::paused:
            v = "paused";
            break;
        case multipass::hyperv::hcs::ComputeSystemState::running:
            v = "running";
            break;
        case multipass::hyperv::hcs::ComputeSystemState::saved_as_template:
            v = "saved_as_template";
            break;
        case multipass::hyperv::hcs::ComputeSystemState::stopped:
            v = "stopped";
            break;
        case multipass::hyperv::hcs::ComputeSystemState::unknown:
            v = "unknown";
            break;
        }

        return format_to(ctx.out(), "{}", v);
    }
};

#endif
