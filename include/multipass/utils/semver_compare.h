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

#include <compare>
#include <string>

namespace multipass
{
struct opaque_semver
{
    std::string value;
};

inline namespace literals
{
[[nodiscard]] inline opaque_semver operator"" _semver(const char* value, std::size_t len)
{
    const auto sv = std::string_view{value, len};
    return opaque_semver{std::string{sv}};
}
} // namespace literals

[[nodiscard]] std::strong_ordering operator<=>(const opaque_semver& lhs, const opaque_semver& rhs);
} // namespace multipass
