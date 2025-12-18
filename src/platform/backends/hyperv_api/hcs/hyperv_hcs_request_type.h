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

#include <string_view>

#include <hyperv_api/format_as_mixin.h>

namespace multipass::hyperv::hcs
{

struct HcsRequestType : FormatAsMixin<HcsRequestType>
{
    [[nodiscard]] operator std::string_view() const
    {
        return value;
    }

    [[nodiscard]] static HcsRequestType Add()
    {
        return {"Add"};
    }

    [[nodiscard]] static HcsRequestType Remove()
    {
        return {"Remove"};
    }

    [[nodiscard]] static HcsRequestType Update()
    {
        return {"Update"};
    }

    [[nodiscard]] bool operator==(const HcsRequestType& rhs) const
    {
        return value == rhs.value;
    }

private:
    HcsRequestType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcs
