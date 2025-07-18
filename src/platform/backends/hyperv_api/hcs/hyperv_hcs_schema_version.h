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

#include <algorithm>
#include <array>

#include <fmt/xchar.h>

#include <multipass/singleton.h>

namespace multipass::hyperv::hcs
{

/**
 * Host Compute System schema versions
 * @ref https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#Schema-Version-Map
 */
enum class HcsSchemaVersion
{
    // Windows 10 SDK, version 1809 (10.0.17763.0)
    v20 = 20,
    // Windows 10 SDK, version 1809 (10.0.17763.0)
    v21 = 21,
    // Windows 10 SDK, version 1903 (10.0.18362.1)
    v22 = 22,
    // Windows 10 SDK, version 2004 (10.0.19041.0)
    v23 = 23,
    // Windows Server 2022 (OS build 20348.169)
    v24 = 24,
    // Windows Server 2022 (OS build 20348.169)
    v25 = 25,
    // Windows 11 SDK, version 21H2 (10.0.22000.194)
    v26 = 26
};

struct SchemaUtils : public Singleton<SchemaUtils>
{
    SchemaUtils(const Singleton<SchemaUtils>::PrivatePass&) noexcept;
    // ---------------------------------------------------------

    /**
     * Retrieve the supported HCS schema version by the host.
     */
    [[nodiscard]] virtual HcsSchemaVersion get_os_supported_schema_version() const;
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for HcsRequest
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HcsSchemaVersion, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HcsSchemaVersion& param, FormatContext& ctx) const ->
        typename FormatContext::iterator;
};
