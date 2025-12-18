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

#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

#include <array>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcn::CreateEndpointParameters;

template <std::size_t N>
struct universal_literal
{
    std::array<char, N> narrow{};
    std::array<wchar_t, N> wide{};

    consteval universal_literal(const char (&str)[N])
    {
        for (std::size_t i = 0; i < N; ++i)
        {
            auto c = static_cast<unsigned char>(str[i]);
            if (c > 127)
                throw "non-ASCII character in universal_literal";

            narrow[i] = str[i];
            wide[i] = static_cast<wchar_t>(c);
        }
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

/**
 * NOTE: returns by const auto6 to leverage static storage duration of Lit.
 */
template <universal_literal Lit>
constexpr const auto& operator""_unv()
{
    return Lit;
}

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<CreateEndpointParameters, Char>::format(const CreateEndpointParameters& params,
                                                            FormatContext& ctx) const
    -> FormatContext::iterator
{
    static constexpr universal_literal json_template{R"json(
    {{
        "SchemaVersion": {{
            "Major": 2,
            "Minor": 16
        }},
        "HostComputeNetwork": "{0}",
        "Policies": [],
        "MacAddress" : {1}
    }})json"};

    return fmt::format_to(ctx.out(),
                          json_template.as<Char>(),
                          maybe_widen{params.network_guid},
                          params.mac_address ? fmt::format(R"("{}")"_unv.as<Char>(),
                                                           maybe_widen{params.mac_address.value()})
                                             : "null"_unv.as<Char>());
}

template auto fmt::formatter<CreateEndpointParameters, char>::format<fmt::format_context>(
    const CreateEndpointParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateEndpointParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateEndpointParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
