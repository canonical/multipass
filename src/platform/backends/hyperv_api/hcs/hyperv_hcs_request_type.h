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

#ifndef MULTIPASS_HYPERV_API_HCS_REQUEST_TYPE_H
#define MULTIPASS_HYPERV_API_HCS_REQUEST_TYPE_H

#include <string>
#include <string_view>

namespace multipass::hyperv::hcs
{

struct HcsRequestType
{
    operator std::string_view() const
    {
        return value;
    }

    operator std::string() const
    {
        return std::string{value};
    }

    constexpr static auto Add()
    {
        return HcsRequestType{"Add"};
    }

    constexpr static auto Remove()
    {
        return HcsRequestType{"Remove"};
    }

    constexpr static auto Update(){
        return HcsRequestType{"Update"};
    }

private:
    constexpr HcsRequestType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcs

#endif
