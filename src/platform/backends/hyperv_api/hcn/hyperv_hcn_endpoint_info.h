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

#include <hyperv_api/hcn/hyperv_hcn_dns.h>

#include <optional>
#include <string>

namespace multipass::hyperv::hcn
{
struct HcnEndpointInfo
{
    std::string guid;
    std::string network_guid;

    /**
     * DNS settings (suffix, search list, servers) the endpoint inherited
     * from its network.
     */
    std::optional<HcnDns> dns;
};

} // namespace multipass::hyperv::hcn
