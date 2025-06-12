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

#ifndef MULTIPASS_HYPERV_API_HCS_NETWORK_ADAPTER_H
#define MULTIPASS_HYPERV_API_HCS_NETWORK_ADAPTER_H

#include <fmt/xchar.h>

#include <string>

namespace multipass::hyperv::hcs
{

struct HcsNetworkAdapter
{
    std::string instance_guid;
    std::string endpoint_guid;
    std::string mac_address;
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for HcnNetworkPolicy
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::HcsNetworkAdapter, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::HcsNetworkAdapter& policy, FormatContext& ctx) const
        -> typename FormatContext::iterator;
};

#endif // MULTIPASS_HYPERV_API_HCS_SCSI_DEVICE_H
