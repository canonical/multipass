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

#include <multipass/exceptions/formatted_exception_base.h>
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
#include <ztd/out_ptr.hpp>

#include <cassert>
#include <string>
#include <type_traits>

using ztd::out_ptr::out_ptr;

namespace multipass::hyperv::hcn
{

struct GuidParseError : multipass::FormattedExceptionBase<>
{
    using FormattedExceptionBase<>::FormattedExceptionBase;
};

namespace
{

using UniqueHcnNetwork =
    std::unique_ptr<std::remove_pointer_t<HCN_NETWORK>, decltype(HCNAPITable::CloseNetwork)>;
using UniqueHcnEndpoint =
    std::unique_ptr<std::remove_pointer_t<HCN_ENDPOINT>, decltype(HCNAPITable::CloseEndpoint)>;
using UniqueCotaskmemString = std::unique_ptr<wchar_t, decltype(HCNAPITable::CoTaskMemFree)>;

namespace mpl = logging;
using lvl = mpl::Level;

// ---------------------------------------------------------

/**
 * Category for the log messages.
 */
constexpr static auto kLogCategory = "HyperV-HCN-Wrapper";

// ---------------------------------------------------------

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_str GUID in string form, either 36 characters
 *                  (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 */
auto guid_from_string(const std::wstring& guid_wstr) -> ::GUID
{
    constexpr static auto kGUIDLength = 36;
    constexpr static auto kGUIDLengthWithBraces = kGUIDLength + 2;

    const auto input = [&guid_wstr]() {
        switch (guid_wstr.length())
        {
        case kGUIDLength:
            // CLSIDFromString requires GUIDs to be wrapped with braces.
            return fmt::format(L"{{{}}}", guid_wstr);
        case kGUIDLengthWithBraces:
        {
            if (*guid_wstr.begin() != L'{' || *std::prev(guid_wstr.end()) != L'}')
            {
                throw GuidParseError{"GUID string either does not start or end with a brace."};
            }
            return guid_wstr;
        }
        }
        throw GuidParseError{"Invalid length for a GUID string ({}).", guid_wstr.length()};
    }();

    ::GUID guid = {};

    const auto result = CLSIDFromString(input.c_str(), &guid);

    if (FAILED(result))
    {
        throw GuidParseError{"Failed to parse the GUID string ({}).", result};
    }

    return guid;
}

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_str GUID in string form, either 36 characters
 *                  (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 */
auto guid_from_string(const std::string& guid_wstr) -> ::GUID
{
    const std::wstring v = maybe_widen{guid_wstr};
    return guid_from_string(v);
}

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
    UniqueCotaskmemString result_msgbuf{};

    // Perform the operation. The last argument of the all HCN operations (except
    // HcnClose*) is ErrorRecord, which is a JSON-formatted document emitted by
    // the API describing the error happened. Therefore, we can streamline all API
    // calls through perform_operation to perform co
    const auto result =
        ResultCode{fn(std::forward<Args>(args)..., out_ptr(result_msgbuf, api.CoTaskMemFree))};

    mpl::debug(kLogCategory,
               "perform_hcn_operation(...) > fn: {}, result: {}",
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

    UniqueHcnNetwork network{};
    const auto result = perform_hcn_operation(api,
                                              api.OpenNetwork,
                                              guid_from_string(network_guid),
                                              out_ptr(network, api.CloseNetwork));
    if (!result)
    {
        mpl::error(kLogCategory, "open_network() > HcnOpenNetwork failed with {}!", result.code);
    }
    return network;
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
    constexpr static auto network_settings_template = LR"""(
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
                    fmt::arg(L"Name", maybe_widen{params.name}),
                    fmt::arg(L"Type", maybe_widen{std::string{params.type}}),
                    fmt::arg(L"Flags", fmt::underlying(params.flags)),
                    fmt::arg(L"Ipams", fmt::join(params.ipams, L",")),
                    fmt::arg(L"Policies", fmt::join(params.policies, L",")));

    UniqueHcnNetwork network{};
    const auto result = perform_hcn_operation(api,
                                              api.CreateNetwork,
                                              guid_from_string(params.guid),
                                              network_settings.c_str(),
                                              out_ptr(network, api.CloseNetwork));

    if (!result)
    {
        mpl::log(hcn_errc_to_log_level(result),
                 kLogCategory,
                 "HCNWrapper::create_network(...) > HcnCreateNetwork failed with {}",
                 result.code);
    }

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
    constexpr static auto endpoint_settings_template = LR"(
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
    const auto endpoint_settings =
        fmt::format(endpoint_settings_template,
                    fmt::arg(L"HostComputeNetwork", maybe_widen{params.network_guid}),
                    fmt::arg(L"MacAddress",
                             params.mac_address
                                 ? fmt::format(L"\"{}\"", maybe_widen{params.mac_address.value()})
                                 : L"null"));
    UniqueHcnEndpoint endpoint{};
    const auto result = perform_hcn_operation(api,
                                              api.CreateEndpoint,
                                              network.get(),
                                              guid_from_string(params.endpoint_guid),
                                              endpoint_settings.c_str(),
                                              out_ptr(endpoint, api.CloseEndpoint));
    return result;
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_endpoint(const std::string& endpoint_guid) const
{
    mpl::debug(kLogCategory,
               "HCNWrapper::delete_endpoint(...) > endpoint_guid: {} ",
               endpoint_guid);
    return perform_hcn_operation(api, api.DeleteEndpoint, guid_from_string(endpoint_guid));
}

} // namespace multipass::hyperv::hcn
