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

#include <concepts>
#include <filesystem>
#include <string>
#include <utility>

namespace multipass
{

/// A filesystem path normalized to use the platform's preferred directory separators.
struct NativePath
{
    template <typename... Args>
        requires std::constructible_from<std::filesystem::path, Args...>
    NativePath(Args&&... arg) : value{std::forward<Args>(arg)...}
    {
        value.make_preferred();
    }

    operator std::filesystem::path() const
    {
        return value;
    }

    NativePath(const NativePath&) = default;
    NativePath(NativePath&&) = default;

    NativePath& operator=(const NativePath&) = default;
    NativePath& operator=(NativePath&&) = default;

    template <typename T>
        requires std::assignable_from<std::filesystem::path&, T>
    NativePath& operator=(T&& v)
    {
        value = std::forward<T>(v);
        value.make_preferred();
        return *this;
    }

    [[nodiscard]] const std::filesystem::path& get() const noexcept
    {
        return value;
    }

    [[nodiscard]] std::wstring wstring() const
    {
        return value.wstring();
    }
    bool empty() const
    {
        return value.empty();
    }
    auto extension() const
    {
        return value.extension();
    }
    auto parent_path() const
    {
        return value.parent_path();
    }

    friend bool operator==(const NativePath&, const NativePath&) = default;
    bool operator==(const std::filesystem::path& p) const
    {
        return value == p;
    }

private:
    std::filesystem::path value;
};
} // namespace multipass

template <typename Char>
struct fmt::formatter<multipass::NativePath, Char> : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::NativePath&, FormatContext&) const -> FormatContext::iterator;
};
