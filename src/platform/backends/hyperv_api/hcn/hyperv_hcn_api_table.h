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

// clang-format off
#include <windows.h>
#include <computenetwork.h>
#include <objbase.h> // for CoTaskMemFree
// clang-format on

#include <fmt/format.h>
#include <functional>

namespace multipass::hyperv::hcn
{

/**
 * API function table for the Host Compute Network API
 */
struct HCNAPITable
{
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcncreatenetwork
    std::function<decltype(HcnCreateNetwork)> CreateNetwork = &HcnCreateNetwork;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcnopennetwork
    std::function<decltype(HcnOpenNetwork)> OpenNetwork = &HcnOpenNetwork;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcndeletenetwork
    std::function<decltype(HcnDeleteNetwork)> DeleteNetwork = &HcnDeleteNetwork;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcnclosenetwork
    std::function<decltype(HcnCloseNetwork)> CloseNetwork = &HcnCloseNetwork;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcncreateendpoint
    std::function<decltype(HcnCreateEndpoint)> CreateEndpoint = &HcnCreateEndpoint;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcnopenendpoint
    std::function<decltype(HcnOpenEndpoint)> OpenEndpoint = &HcnOpenEndpoint;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcndeleteendpoint
    std::function<decltype(HcnDeleteEndpoint)> DeleteEndpoint = &HcnDeleteEndpoint;
    // @ref https://learn.microsoft.com/en-us/virtualization/api/hcn/reference/hcncloseendpoint
    std::function<decltype(HcnCloseEndpoint)> CloseEndpoint = &HcnCloseEndpoint;
    // @ref
    // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cotaskmemfree
    std::function<decltype(::CoTaskMemFree)> CoTaskMemFree = &::CoTaskMemFree;
};

} // namespace multipass::hyperv::hcn

template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::HCNAPITable, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::HCNAPITable& api, FormatContext& ctx) const
    {
        return fmt::format_to(
            ctx.out(),
            "CreateNetwork: ({}) | OpenNetwork: ({}) | DeleteNetwork: ({}) | CloseNetwork: ({}) | "
            "CreateEndpoint: ({}) | OpenEndpoint: ({}) | DeleteEndpoint: ({}) | CloseEndpoint: "
            "({}) | CoTaskMemFree: ({})",
            static_cast<bool>(api.CreateNetwork),
            static_cast<bool>(api.OpenNetwork),
            static_cast<bool>(api.DeleteNetwork),
            static_cast<bool>(api.CloseNetwork),
            static_cast<bool>(api.CreateEndpoint),
            static_cast<bool>(api.OpenEndpoint),
            static_cast<bool>(api.DeleteEndpoint),
            static_cast<bool>(api.CloseEndpoint),
            static_cast<bool>(api.CoTaskMemFree));
    }
};
