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

#include <optional>
#include <string_view>

namespace multipass
{

enum class WindowsFeatureState : int
{
    Enabled = 1,
    Disabled = 2,
    Absent = 3,
};

/**
 * Check if an optional Windows feature is installed.
 *
 * @param feature_name The feature name, e.g. "VirtualMachinePlatform"
 * @return Enum value representing feature's current state, or nullopt on query failure.
 */
std::optional<WindowsFeatureState> get_windows_feature_state(std::wstring_view feature_name);
} // namespace multipass
