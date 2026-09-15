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
 * HcsPath is a strong type that ensures paths use the native format expected by the Hyper-V APIs.

 */
struct HcsPath
{
    template <typename... Args>
        requires std::constructible_from<std::filesystem::path, Args...>
    HcsPath(Args&&... arg) : value{std::forward<Args>(arg)...}
    {
        value.make_preferred();
    }

    template <typename T>
    HcsPath& operator=(T&& v)
    {
        value = std::forward<T>(v);
        value.make_preferred();
        return *this;
    }
    [[nodiscard]] const std::filesystem::path& get() const noexcept
    {
        return value;
    }
    [[nodiscard]] std::string string() const
    {
        return value.string();
    }
    [[nodiscard]] std::wstring wstring() const
    {
        return value.wstring();
    }
    friend bool operator==(const HcsPath&, const HcsPath&) = default;

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
