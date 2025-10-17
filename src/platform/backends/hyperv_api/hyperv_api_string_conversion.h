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

#include <codecvt>
#include <locale>
#include <string>
#include <string_view>

#include <fmt/xchar.h>

namespace multipass::hyperv
{

struct universal_string_literal_helper
{
    std::string_view narrow;
    std::wstring_view wide;

    template <typename Char>
    [[nodiscard]] auto as() const;

    template <>
    [[nodiscard]] constexpr auto as<std::string_view::value_type>() const
    {
        return narrow;
    }

    template <>
    [[nodiscard]] constexpr auto as<std::wstring_view::value_type>() const
    {
        return wide;
    }
};

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
 *  Formatter type specialization for CreateNetworkParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::maybe_widen, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::maybe_widen& params, FormatContext& ctx) const
    {
        constexpr static Char fmt_str[] = {'{', '}', '\0'};
        using const_sref_type = const std::basic_string<typename FormatContext::char_type>&;
        return fmt::format_to(ctx.out(), fmt_str, static_cast<const_sref_type>(params));
    }
};

#define MULTIPASS_UNIVERSAL_LITERAL(X)                                                             \
    multipass::hyperv::universal_string_literal_helper                                             \
    {                                                                                              \
        "" X, L"" X                                                                                \
    }
