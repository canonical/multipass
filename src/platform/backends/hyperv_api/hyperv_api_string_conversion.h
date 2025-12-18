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
#include <string_view>

#include <fmt/xchar.h>

namespace multipass::hyperv
{

inline constexpr auto to_wstring(auto&& value)
{
    // TODO: This has been deprecated. Replace with Boost.nowide or equivalent.
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(value.data(),
                                                                         value.data() +
                                                                             value.size());
}

namespace detail
{

template <typename Char, std::size_t N>
struct string_literal
{
    std::array<Char, N> storage;

    consteval string_literal(const char (&str)[N])
    {
        if constexpr (std::is_same_v<Char, char>)
            std::ranges::copy(str, storage.begin());
        else
        {
            std::ranges::transform(str, storage.begin(), [](char c) {
                auto uc = static_cast<unsigned char>(c);
                if (uc > 127)
                    throw "non-ASCII character in universal_literal";
                return static_cast<wchar_t>(uc);
            });
        }
    }

    constexpr operator fmt::basic_string_view<Char>() const
    {
        return fmt::basic_string_view<Char>{storage.data(), N - 1};
    }

    constexpr operator std::basic_string_view<Char>() const
    {
        return std::basic_string_view<Char>{storage.data(), N - 1};
    }
};

template <typename Char, std::size_t N>
class formattable_string_literal : public string_literal<Char, N>
{
    template <typename T>
    static constexpr decltype(auto) adapt(T&& arg);

public:
    using string_literal<Char, N>::string_literal;

    template <typename FormatContext, typename... Args>
    constexpr auto format_to(FormatContext& ctx, Args&&... args) const
    {
        return fmt::format_to(ctx.out(), fmt::runtime(*this), adapt(std::forward<Args>(args))...);
    }

    template <typename... Args>
    constexpr auto format(Args&&... args) const
    {
        return fmt::format(fmt::runtime(*this), adapt(std::forward<Args>(args))...);
    }
};

template <typename T>
concept narrow_string_like = std::is_convertible_v<T, std::string_view>;

template <typename Char, std::size_t N>
template <typename T>
inline constexpr decltype(auto) formattable_string_literal<Char, N>::adapt(T&& arg)
{
    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<Char, wchar_t> && narrow_string_like<D>)
    {
        return to_wstring(std::string_view{std::forward<T>(arg)});
    }
    else
        return std::forward<T>(arg);
}
} // namespace detail

template <typename Char, std::size_t N>
consteval auto string_literal(const char (&str)[N]) -> detail::formattable_string_literal<Char, N>
{
    return detail::formattable_string_literal<Char, N>{str};
}

} // namespace multipass::hyperv
