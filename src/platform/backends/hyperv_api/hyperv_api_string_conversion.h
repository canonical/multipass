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

#ifndef MULTIPASS_HYPERV_API_STRING_CONVERSION_H
#define MULTIPASS_HYPERV_API_STRING_CONVERSION_H

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
    auto as() const;

    template <>
    auto as<std::string_view::value_type>() const
    {
        return narrow;
    }

    template <>
    auto as<std::wstring_view::value_type>() const
    {
        return wide;
    }
};

struct maybe_widen
{
    explicit maybe_widen(const std::string& v) : narrow(v)
    {
    }

    operator const std::string&() const
    {
        return narrow;
    }

    operator std::wstring() const
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
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::maybe_widen& params, FormatContext& ctx) const
    {
        constexpr static Char fmt_str[] = {'{', '}', '\0'};
        using const_sref_type = const std::basic_string<typename FormatContext::char_type>&;
        return format_to(ctx.out(), fmt_str, static_cast<const_sref_type>(params));
    }
};

#define MULTIPASS_UNIVERSAL_LITERAL(X)                                                                                 \
    multipass::hyperv::universal_string_literal_helper                                                                 \
    {                                                                                                                  \
        "" X, L"" X                                                                                                    \
    }

#endif // MULTIPASS_HYPERV_API_STRING_CONVERSION_H
