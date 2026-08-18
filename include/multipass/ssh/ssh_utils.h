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
#include <algorithm>
#include <array>
#include <string_view>

namespace multipass
{
namespace SSH
{

struct SignalMapping
{
    std::string_view name;
    int offset;
};

// The signal numbers vary in some architectures, but not in any we support
constexpr std::array signal_map{
    SignalMapping{"HUP", 1},
    SignalMapping{"INT", 2},
    SignalMapping{"QUIT", 3},
    SignalMapping{"ILL", 4},
    SignalMapping{"ABRT", 6},
    SignalMapping{"FPE", 8},
    SignalMapping{"KILL", 9},
    SignalMapping{"USR1", 10},
    SignalMapping{"SEGV", 11},
    SignalMapping{"USR2", 12},
    SignalMapping{"PIPE", 13},
    SignalMapping{"ALRM", 14},
    SignalMapping{"TERM", 15},
};

/**
 * @brief Maps SSH channel exit signal names to shell exit codes (128 + signal).
 *
 * @details
 * This mapping intentionally uses fixed, target-independent offsets rather than
 * native system macros like `SIGKILL` or `SIGSEGV` for the following reasons:
 *
 * 1. Signals are received from a remote Linux guest, but this code must compile and run on
 * non-Linux hosts (macOS, Windows) without relying on target-specific `<csignal>` or Linux headers.
 *
 * 2. The supported subset of signal names is defined directly by the SSH connection protocol
 * specification (RFC 4254, Section 6.9/10).
 *
 * 3.  While RFC 4254 allows implementation-defined additional signal names, OpenSSH only supports
 * this core POSIX subset, so further mappings are omitted.
 *
 * @param sig The signal name string received over the SSH channel (e.g., "KILL", "TERM").
 * @return The calculated exit status (128 + signal offset), or 128 if unknown/null.
 * @see https://www.rfc-editor.org/info/rfc4254/#section-6.9
 */
inline auto signal_to_exit_code(const char* sig) -> int
{
    constexpr auto base_signal_code{128};

    if (!sig || !*sig)
        return base_signal_code;

    const std::string_view signal{sig};

    const auto it = std::find_if(signal_map.begin(), signal_map.end(), [signal](const auto& entry) {
        return entry.name == signal;
    });

    if (it != signal_map.end())
        return base_signal_code + it->offset;

    return base_signal_code;
}
} // namespace SSH
} // namespace multipass
