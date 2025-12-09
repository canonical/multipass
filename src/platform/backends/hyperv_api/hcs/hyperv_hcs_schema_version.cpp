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

#include <platform/platform_win.h>

#include <optional>

namespace
{

// https://www.wikiwand.com/en/articles/Windows_10_version_history
// https://www.wikiwand.com/en/articles/Windows_11_version_history
// https://www.wikiwand.com/en/articles/List_of_Microsoft_Windows_versions
enum class WindowsBuildNumbers : std::uint32_t
{
    // April 2018 Update, April 30, 2018
    win10_1809 = 17763,
    // May 2019 Update, May 21, 2019
    win10_19H1 = 18362,
    // May 2020 Update, May 27, 2020
    win10_20H1 = 19041,
    // Codename "Vibranium", August 18, 2021
    srv22_21H2 = 20348,
    // Codename "Sun Valley", October 5, 2021
    win11_21H2 = 22000
};

struct SchemaVersionBuildNumberMapping
{
    multipass::hyperv::hcs::HcsSchemaVersion version;
    WindowsBuildNumbers required_build_number;

    static bool descending(const SchemaVersionBuildNumberMapping& lhs,
                           const SchemaVersionBuildNumberMapping& rhs)
    {
        if (lhs.required_build_number != rhs.required_build_number)
            return lhs.required_build_number > rhs.required_build_number;

        return lhs.version > rhs.version;
    }
};
} // namespace

namespace multipass::hyperv::hcs
{

SchemaUtils::SchemaUtils(const Singleton<SchemaUtils>::PrivatePass& pass) noexcept
    : Singleton<SchemaUtils>::Singleton{pass}
{
}

// ---------------------------------------------------------

HcsSchemaVersion SchemaUtils::get_os_supported_schema_version() const
{
    const static auto cached_schema_version = []() -> std::optional<HcsSchemaVersion> {
        if (const auto winver = platform::get_windows_version())
        {

            std::array schema_version_mappings{
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v20,
                                                WindowsBuildNumbers::win10_1809},
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v21,
                                                WindowsBuildNumbers::win10_1809},
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v22,
                                                WindowsBuildNumbers::win10_19H1},
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v23,
                                                WindowsBuildNumbers::win10_19H1},
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v24,
                                                WindowsBuildNumbers::srv22_21H2},
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v25,
                                                WindowsBuildNumbers::srv22_21H2},
                SchemaVersionBuildNumberMapping{HcsSchemaVersion::v26,
                                                WindowsBuildNumbers::win11_21H2}};

            // Sort descending, based on build number and version (when build number is equal)
            std::sort(schema_version_mappings.begin(),
                      schema_version_mappings.end(),
                      SchemaVersionBuildNumberMapping::descending);

            const auto it = std::ranges::find_if(schema_version_mappings, [winver](const auto& m) {
                return fmt::underlying(m.required_build_number) <= winver->build;
            });

            if (it != schema_version_mappings.end())
                return it->version;
        }
        return {};
    }();

    // If unable to determine default to the lowest possible schema version
    return cached_schema_version.value_or(HcsSchemaVersion::v20);
}
} // namespace multipass::hyperv::hcs

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::HcsSchemaVersion;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsSchemaVersion, Char>::format(const HcsSchemaVersion& schema_version,
                                                    FormatContext& ctx) const
    -> FormatContext::iterator
{
    constexpr auto fmt_str = MULTIPASS_UNIVERSAL_LITERAL("{}");

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

    return fmt::format_to(ctx.out(), fmt_str.as<Char>(), result.as<Char>());
}

template auto fmt::formatter<HcsSchemaVersion, char>::format<fmt::format_context>(
    const HcsSchemaVersion&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsSchemaVersion, wchar_t>::format<fmt::wformat_context>(
    const HcsSchemaVersion&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
