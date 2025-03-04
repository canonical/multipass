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

#pragma comment(lib, "computecore.lib")
#pragma comment(lib, "computenetwork.lib")
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

    mpl::log(lvl::trace,
             kLogCategory,
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
    mpl::log(lvl::trace, kLogCategory, "open_network(...) > network_guid: {} ", network_guid);
    HCN_NETWORK network{nullptr};

    const auto result = perform_hcn_operation(api, api.OpenNetwork, guid_from_string(network_guid), &network);
    if (!result)
    {
        mpl::log(lvl::error, kLogCategory, "open_network() > HcnOpenNetwork failed with {}!", result.code);
    }
    return UniqueHcnNetwork{network, api.CloseNetwork};
}

} // namespace

// ---------------------------------------------------------

HCNWrapper::HCNWrapper(const HCNAPITable& api_table) : api{api_table}
{
    mpl::log(lvl::trace, kLogCategory, "HCNWrapper::HCNWrapper(...): api_table: {}", api);
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_network(const CreateNetworkParameters& params) const
{
    mpl::log(lvl::trace, kLogCategory, "HCNWrapper::create_network(...) > params: {} ", params);

    /**
     * HcnCreateNetwork settings JSON template
     */
    constexpr auto network_settings_template = LR"""(
    {{
        "Name": "{0}",
        "Type": "ICS",
        "Subnets" : [
            {{
                "GatewayAddress": "{2}",
                "AddressPrefix" : "{1}",
                "IpSubnets" : [
                    {{
                        "IpAddressPrefix": "{1}"
                    }}
                ]
            }}
        ],
        "IsolateSwitch": true,
        "Flags" : 265
    }}
    )""";

    // Render the template
    const auto network_settings = fmt::format(network_settings_template,
                                              string_to_wstring(params.name),
                                              string_to_wstring(params.subnet),
                                              string_to_wstring(params.gateway));

    HCN_NETWORK network{nullptr};
    const auto result = perform_hcn_operation(api,
                                              api.CreateNetwork,
                                              guid_from_string(params.guid),
                                              network_settings.c_str(),
                                              &network);

    if (!result)
    {
        // FIXME: Also include the result error message, if any.
        mpl::log(lvl::error,
                 kLogCategory,
                 "HCNWrapper::create_network(...) > HcnCreateNetwork failed with {}!",
                 result.code);
    }

    [[maybe_unused]] UniqueHcnNetwork _{network, api.CloseNetwork};
    return result;
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_network(const std::string& network_guid) const
{
    mpl::log(lvl::trace, kLogCategory, "HCNWrapper::delete_network(...) > network_guid: {}", network_guid);
    return perform_hcn_operation(api, api.DeleteNetwork, guid_from_string(network_guid));
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_endpoint(const CreateEndpointParameters& params) const
{
    mpl::log(lvl::trace, kLogCategory, "HCNWrapper::create_endpoint(...) > params: {} ", params);

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
        "HostComputeNetwork": "{0}",
        "Policies": [
        ],
        "IpConfigurations": [
            {{
                "IpAddress": "{1}"
            }}
        ]
    }})";

    // Render the template
    const auto endpoint_settings = fmt::format(endpoint_settings_template,
                                               string_to_wstring(params.network_guid),
                                               string_to_wstring(params.endpoint_ipvx_addr));
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
    mpl::log(lvl::trace, kLogCategory, "HCNWrapper::delete_endpoint(...) > endpoint_guid: {} ", endpoint_guid);
    return perform_hcn_operation(api, api.DeleteEndpoint, guid_from_string(endpoint_guid));
}

} // namespace multipass::hyperv::hcn
