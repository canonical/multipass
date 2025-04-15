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

#ifndef MULTIPASS_HYPERV_API_HCN_IPAM_H
#define MULTIPASS_HYPERV_API_HCN_IPAM_H

#include <hyperv_api/hcn/hyperv_hcn_ipam_type.h>
#include <hyperv_api/hcn/hyperv_hcn_subnet.h>

#include <vector>

namespace multipass::hyperv::hcn
{

struct HcnIpam
{
    /**
     * Type of the IPAM
     */
    HcnIpamType type{HcnIpamType::Static()};

    /**
     * Defined subnet ranges for the IPAM
     */
    std::vector<HcnSubnet> subnets;
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for HcnIpam
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HcnIpam, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HcnIpam& ipam, FormatContext& ctx) const
    {
        return format_to(ctx.out(),
                         "Type: ({}) | Subnets: ({})",
                         static_cast<std::string_view>(ipam.type),
                         fmt::join(ipam.subnets, ","));
    }
};

#endif
