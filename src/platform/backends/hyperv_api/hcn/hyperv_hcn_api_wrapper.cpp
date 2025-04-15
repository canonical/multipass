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

#include <hyperv_api/hcn/hyperv_hcn_api_table.h>
#include <hyperv_api/hcn/hyperv_hcn_api_wrapper.h>
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper_interface.h>
#include <hyperv_api/hyperv_api_common.h>

#include <multipass/logging/log.h>
#include <multipass/utils.h>

// clang-format off
#include <windows.h>
#include <computecore.h>
#include <computedefs.h>
#include <computestorage.h>
#include <computenetwork.h>
#include <objbase.h> // HCN API uses CoTaskMem* functions to allocate memory.
// clang-format on

#include <fmt/xchar.h>

#include <cassert>
#include <string>
#include <type_traits>

namespace multipass::hyperv::hcn
{

namespace
{

using UniqueHcnNetwork = std::unique_ptr<std::remove_pointer_t<HCN_NETWORK>, decltype(HCNAPITable::CloseNetwork)>;
using UniqueHcnEndpoint = std::unique_ptr<std::remove_pointer_t<HCN_ENDPOINT>, decltype(HCNAPITable::CloseEndpoint)>;
using UniqueCotaskmemString = std::unique_ptr<wchar_t, decltype(HCNAPITable::CoTaskMemFree)>;

namespace mpl = logging;
using lvl = mpl::Level;

// ---------------------------------------------------------

/**
 * Category for the log messages.
 */
constexpr auto kLogCategory = "HyperV-HCN-Wrapper";

// ---------------------------------------------------------

/**
 * Perform a Host Compute Network API operation
 *
 * @param fn The API function pointer
 * @param args The arguments to the function
 *
 * @return HCNOperationResult Result of the performed operation
 */
template <typename FnType, typename... Args>
OperationResult perform_hcn_operation(const HCNAPITable& api, const FnType& fn, Args&&... args)
{

    // Ensure that function to call is set.
    if (nullptr == fn)
    {
        assert(0);
        // E_POINTER means "invalid pointer", seems to be appropriate.
        return {E_POINTER, L"Operation function is unbound!"};
    }

    // HCN functions will use CoTaskMemAlloc to allocate the error message buffer
    // so use UniqueCotaskmemString to auto-release it with appropriate free
    // function.

    wchar_t* result_msg_out{nullptr};

    // Perform the operation. The last argument of the all HCN operations (except
    // HcnClose*) is ErrorRecord, which is a JSON-formatted document emitted by
    // the API describing the error happened. Therefore, we can streamline all API
    // calls through perform_operation to perform co
    const auto result = ResultCode{fn(std::forward<Args>(args)..., &result_msg_out)};

    UniqueCotaskmemString result_msgbuf{result_msg_out, api.CoTaskMemFree};

    mpl::debug(kLogCategory,
               "perform_operation(...) > fn: {}, result: {}",
               fmt::ptr(fn.template target<void*>()),
               static_cast<bool>(result));

    // Error message is only valid when the operation resulted in an error.
    // Passing a nullptr is well-defined in "< C++23", but it's going to be
    // forbidden afterwards. Going an extra mile just to be future-proof.
    return {result, {result_msgbuf ? result_msgbuf.get() : L""}};
}

// ---------------------------------------------------------

/**
 * Open an existing Host Compute Network and return a handle to it.
 *
 * This function is used for altering network resources, e.g. adding a new
 * endpoint.
 *
 * @param api The HCN API table
 * @param network_guid GUID of the network to open
 *
 * @return UniqueHcnNetwork Unique handle to the network. Non-nullptr when successful.
 */
UniqueHcnNetwork open_network(const HCNAPITable& api, const std::string& network_guid)
{
    mpl::debug(kLogCategory, "open_network(...) > network_guid: {} ", network_guid);
    HCN_NETWORK network{nullptr};

    const auto result = perform_hcn_operation(api, api.OpenNetwork, guid_from_string(network_guid), &network);
    if (!result)
    {
        mpl::error(kLogCategory, "open_network() > HcnOpenNetwork failed with {}!", result.code);
    }
    return UniqueHcnNetwork{network, api.CloseNetwork};
}

/**
 * Determine the log severity level for a HCN error.
 *
 * @param [in] result Operation result
 * @return mpl::Level The determined severity level
 */
auto hcn_errc_to_log_level(const OperationResult& result)
{
    /**
     * Some of the errors are "expected", e.g. a network may be already
     * exist and that's not necessarily an error.
     */
    switch (static_cast<HRESULT>(result.code))
    {
    case HCN_E_NETWORK_ALREADY_EXISTS:
        return mpl::Level::debug;
    }

    return mpl::Level::error;
}

/**
 * For each element in @p elem, apply @p op and
 * aggregate the result. Then, join all by comma.
 *
 * @tparam T Element type
 * @tparam Func Function type
 * @param [in] elems Elements
 * @param [in] op Operation to apply
 * @return Comma-separated list of transformed elements
 */
template <typename T, typename Func>
auto transform_join(const std::vector<T>& elems, Func op)
{
    std::vector<std::wstring> result;
    std::transform(elems.begin(), elems.end(), std::back_inserter(result), op);
    return fmt::format(L"{}", fmt::join(result, L","));
}

/**
 * Format a HcnRoute
 *
 * @param [in] route Route to format
 * @return HCN JSON representation of @p route
 */
std::wstring format_route(const HcnRoute& route)
{
    constexpr auto route_template = LR"""(
        {{
            "NextHop": "{NextHop}",
            "DestinationPrefix": "{DestinationPrefix}",
            "Metric": {Metric}
        }}
    )""";

    return fmt::format(route_template,
                       fmt::arg(L"NextHop", string_to_wstring(route.next_hop)),
                       fmt::arg(L"DestinationPrefix", string_to_wstring(route.destination_prefix)),
                       fmt::arg(L"Metric", route.metric));
}

/**
 * Format a HcnSubnet
 *
 * @param [in] subnet Subnet to format
 * @return HCN JSON representation of @p subnet
 */
std::wstring format_subnet(const HcnSubnet& subnet)
{
    constexpr auto subnet_template = LR"""(
            {{
                "Policies": [],
                "Routes" : [
                    {Routes}
                ],
                "IpAddressPrefix" : "{IpAddressPrefix}",
                "IpSubnets": null
            }}
        )""";

    return fmt::format(subnet_template,
                       fmt::arg(L"IpAddressPrefix", string_to_wstring(subnet.ip_address_prefix)),
                       fmt::arg(L"Routes", transform_join(subnet.routes, format_route)));
}

/**
 * Format a HcnIpam
 *
 * @param [in] ipam IPAM to format
 * @return HCN JSON representation of @p ipam
 */
std::wstring format_ipam(const HcnIpam& ipam)
{
    constexpr auto ipam_template = LR"""(
        {{
            "Type": "{Type}",
            "Subnets": [
                {Subnets}
            ]
        }}
    )""";

    return fmt::format(ipam_template,
                       fmt::arg(L"Type", string_to_wstring(std::string{ipam.type})),
                       fmt::arg(L"Subnets", transform_join(ipam.subnets, format_subnet)));
}

struct NetworkPolicySettingsFormatters
{
    std::wstring operator()(const HcnNetworkPolicyNetAdapterName& policy)
    {
        constexpr auto netadaptername_settings_template = LR"""(
            "NetworkAdapterName": "{NetworkAdapterName}"
        )""";

        return fmt::format(netadaptername_settings_template,
                           fmt::arg(L"NetworkAdapterName", string_to_wstring(policy.net_adapter_name)));
    }
};

std::wstring format_network_policy(const HcnNetworkPolicy& policy)
{
    constexpr auto network_policy_template = LR"""(
        {{
            "Type": "{Type}",
            "Settings": {{
                {Settings}
            }}
        }}
    )""";
    return fmt::format(network_policy_template,
                       fmt::arg(L"Type", string_to_wstring(policy.type)),
                       fmt::arg(L"Settings", std::visit(NetworkPolicySettingsFormatters{}, policy.settings)));
}

} // namespace

// ---------------------------------------------------------

HCNWrapper::HCNWrapper(const HCNAPITable& api_table) : api{api_table}
{
    mpl::debug(kLogCategory, "HCNWrapper::HCNWrapper(...): api_table: {}", api);
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_network(const CreateNetworkParameters& params) const
{
    mpl::debug(kLogCategory, "HCNWrapper::create_network(...) > params: {} ", params);

    /**
     * HcnCreateNetwork settings JSON template
     */
    constexpr auto network_settings_template = LR"""(
    {{
        "SchemaVersion":
        {{
            "Major": 2,
            "Minor": 2
        }},
        "Name": "{Name}",
        "Type": "{Type}",
        "Ipams": [
            {Ipams}
        ],
        "Flags": {Flags},
        "Policies": [
            {Policies}
        ]
    }}
    )""";

    // Render the template
    const auto network_settings =
        fmt::format(network_settings_template,
                    fmt::arg(L"Name", string_to_wstring(params.name)),
                    fmt::arg(L"Type", string_to_wstring(std::string{params.type})),
                    fmt::arg(L"Flags", fmt::underlying(params.flags)),
                    fmt::arg(L"Ipams", transform_join(params.ipams, format_ipam)),
                    fmt::arg(L"Policies", transform_join(params.policies, format_network_policy)));

    HCN_NETWORK network{nullptr};
    const auto result = perform_hcn_operation(api,
                                              api.CreateNetwork,
                                              guid_from_string(params.guid),
                                              network_settings.c_str(),
                                              &network);

    if (!result)
    {
        mpl::log(hcn_errc_to_log_level(result),
                 kLogCategory,
                 "HCNWrapper::create_network(...) > HcnCreateNetwork failed with {}: {}",
                 result.code,
                 std::system_category().message(static_cast<HRESULT>(result.code)));
    }

    [[maybe_unused]] UniqueHcnNetwork _{network, api.CloseNetwork};
    return result;
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_network(const std::string& network_guid) const
{
    mpl::debug(kLogCategory, "HCNWrapper::delete_network(...) > network_guid: {}", network_guid);
    return perform_hcn_operation(api, api.DeleteNetwork, guid_from_string(network_guid));
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_endpoint(const CreateEndpointParameters& params) const
{
    mpl::debug(kLogCategory, "HCNWrapper::create_endpoint(...) > params: {} ", params);

    const auto network = open_network(api, params.network_guid);

    if (nullptr == network)
    {
        return {E_POINTER, L"Could not open the network!"};
    }

    /**
     * HcnCreateEndpoint settings JSON template
     */
    constexpr auto endpoint_settings_template = LR"(
    {{
        "SchemaVersion": {{
            "Major": 2,
            "Minor": 16
        }},
        "HostComputeNetwork": "{HostComputeNetwork}",
        "Policies": [],
        "MacAddress" : {MacAddress}
    }})";

    // Render the template
    const auto endpoint_settings = fmt::format(
        endpoint_settings_template,
        fmt::arg(L"HostComputeNetwork", string_to_wstring(params.network_guid)),
        fmt::arg(L"MacAddress",
                 params.mac_address ? fmt::format(L"\"{}\"", string_to_wstring(params.mac_address.value())) : L"null"));
    HCN_ENDPOINT endpoint{nullptr};
    const auto result = perform_hcn_operation(api,
                                              api.CreateEndpoint,
                                              network.get(),
                                              guid_from_string(params.endpoint_guid),
                                              endpoint_settings.c_str(),
                                              &endpoint);
    [[maybe_unused]] UniqueHcnEndpoint _{endpoint, api.CloseEndpoint};
    return result;
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_endpoint(const std::string& endpoint_guid) const
{
    mpl::debug(kLogCategory, "HCNWrapper::delete_endpoint(...) > endpoint_guid: {} ", endpoint_guid);
    return perform_hcn_operation(api, api.DeleteEndpoint, guid_from_string(endpoint_guid));
}

} // namespace multipass::hyperv::hcn
