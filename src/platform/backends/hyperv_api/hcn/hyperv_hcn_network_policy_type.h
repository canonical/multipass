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

#include <hyperv_api/format_as_mixin.h>

#include <fmt/format.h>

#include <string_view>

namespace multipass::hyperv::hcn
{

/**
 * Strongly-typed string values for network policy types.
 *
 * @ref
 * https://github.com/MicrosoftDocs/Virtualization-Documentation/blob/51b2c0024ce9fc0c9c240fe8e14b170e05c57099/virtualization/api/hcn/HNS_Schema.md?plain=1#L522
 */
struct HcnNetworkPolicyType : FormatAsMixin<HcnNetworkPolicyType>
{
    [[nodiscard]] operator std::string_view() const
    {
        return value;
    }

    /**
     * @since Version 2.0
     */
    [[nodiscard]] static HcnNetworkPolicyType NetAdapterName()
    {
        return {"NetAdapterName"};
    }

    [[nodiscard]] bool operator==(const HcnNetworkPolicyType& rhs) const
    {
        return value == rhs.value;
    }

private:
    HcnNetworkPolicyType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcn
