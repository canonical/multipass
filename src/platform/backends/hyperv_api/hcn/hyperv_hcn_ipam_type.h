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

#ifndef MULTIPASS_HYPERV_API_HCN_IPAM_TYPE_H
#define MULTIPASS_HYPERV_API_HCN_IPAM_TYPE_H

#include <string>
#include <string_view>

namespace multipass::hyperv::hcn
{

struct HcnIpamType
{
    operator std::string_view() const
    {
        return value;
    }

    operator std::string() const
    {
        return std::string{value};
    }

    static inline const auto Dhcp()
    {
        return HcnIpamType{"DHCP"};
    }

    static inline const auto Static()
    {
        return HcnIpamType{"static"};
    }

private:
    HcnIpamType(std::string_view v) : value(v)
    {
    }

    std::string_view value{};
};

} // namespace multipass::hyperv::hcn

#endif
