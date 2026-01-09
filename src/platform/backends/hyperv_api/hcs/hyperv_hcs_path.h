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

#include <fmt/xchar.h>

#include <filesystem>

namespace multipass::hyperv::hcs
{
/**
 * Host Compute System API expects paths to be formatted in a certain way. HcsPath is a strong type
 * that ensures the correct formatting.
 */
struct HcsPath
{
    template <typename... Args>
    HcsPath(Args&&... args) : value{std::forward<Args>(args)...}
    {
    }

    template <typename T>
    HcsPath& operator=(T&& v)
    {
        value = std::forward<T>(v);
        return *this;
    }
    [[nodiscard]] const std::filesystem::path& get() const noexcept
    {
        return value;
    }

private:
    std::filesystem::path value;
};
} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for Path
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HcsPath, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HcsPath&, FormatContext&) const
        -> FormatContext::iterator;
};
