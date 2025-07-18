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
#include <hyperv_api/hcs/hyperv_hcs_schema_version.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::HcsSchemaVersion;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsSchemaVersion, Char>::format(const HcsSchemaVersion& schema_version,
                                                    FormatContext& ctx) const ->
    typename FormatContext::iterator
{
    constexpr static auto fmt_str = MULTIPASS_UNIVERSAL_LITERAL("{}");

    auto result = MULTIPASS_UNIVERSAL_LITERAL("Unknown");

    switch (schema_version)
    {
    case HcsSchemaVersion::v20:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.0");
        break;
    case HcsSchemaVersion::v21:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.1");
        break;
    case HcsSchemaVersion::v22:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.2");
        break;
    case HcsSchemaVersion::v23:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.3");
        break;
    case HcsSchemaVersion::v24:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.4");
        break;
    case HcsSchemaVersion::v25:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.5");
        break;
    case HcsSchemaVersion::v26:
        result = MULTIPASS_UNIVERSAL_LITERAL("v2.6");
        break;
    }

    return format_to(ctx.out(), fmt_str.as<Char>(), result.as<Char>());
}

template auto fmt::formatter<HcsSchemaVersion, char>::format<fmt::format_context>(
    const HcsSchemaVersion&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsSchemaVersion, wchar_t>::format<fmt::wformat_context>(
    const HcsSchemaVersion&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
