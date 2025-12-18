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

#include <array>
#include <codecvt>
#include <locale>
#include <ranges>
#include <string>
#include <string_view>

#include <fmt/xchar.h>

namespace multipass::hyperv
{
/**
 * Compile-time narrow-wide conversion helper.
 *
 * Only works with ASCII characters.
 */
template <std::size_t N>
struct universal_literal
{
    std::array<char, N> narrow{};
    std::array<wchar_t, N> wide{};

    consteval universal_literal(const char (&str)[N])
    {
        std::ranges::copy(str, narrow.begin());
        std::ranges::transform(str, wide.begin(), [](char c) {
            auto uc = static_cast<unsigned char>(c);
            if (uc > 127)
                throw "non-ASCII character in universal_literal";
            return static_cast<wchar_t>(uc);
        });
    }

    template <typename Char>
    [[nodiscard]] constexpr auto as() const noexcept
    {
        if constexpr (std::is_same_v<Char, char>)
            return std::string_view{narrow.data(), N - 1};
        else
            return std::wstring_view{wide.data(), N - 1};
    }
};

// Deduction guide
template <std::size_t N>
universal_literal(const char (&)[N]) -> universal_literal<N>;

namespace literals
{
template <universal_literal Lit>
constexpr const auto& operator""_unv()
{
    return Lit;
}
} // namespace literals

struct maybe_widen
{
    explicit maybe_widen(const std::string& v) : narrow(v)
    {
    }

    [[nodiscard]] operator const std::string&() const
    {
        return narrow;
    }

    [[nodiscard]] operator std::wstring() const
    {
        return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(narrow);
    }

private:
    const std::string& narrow;
};

} // namespace multipass::hyperv

/**
 *  Formatter type specialization for maybe_widen
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::maybe_widen, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::maybe_widen& params, FormatContext& ctx) const
    {
        using const_sref_type = const std::basic_string<typename FormatContext::char_type>&;
        return fmt::formatter<basic_string_view<Char>, Char>::format(
            static_cast<const_sref_type>(params),
            ctx);
    }
};
