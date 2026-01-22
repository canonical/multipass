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

#include <hyperv_api/hcn/hyperv_hcn_api.h>
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcn/hyperv_hcn_endpoint_query.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>

#include <shared/windows/wchar_conversion.h>

#include <windows.h>
#include <computecore.h>
#include <computedefs.h>
#include <computenetwork.h>
#include <computestorage.h>
#include <objbase.h>

#include <boost/json.hpp>
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
inline const HCNAPI& API()
{
    return HCNAPI::instance();
}

struct HcnNetworkCloser
{
    void operator()(HCN_NETWORK p) const noexcept
    {
        (void)API().HcnCloseNetwork(p);
    }
};

using UniqueHcnNetwork = std::unique_ptr<std::remove_pointer_t<HCN_NETWORK>, HcnNetworkCloser>;

struct HcnEndpointCloser
{
    void operator()(HCN_ENDPOINT p) const noexcept
    {
        (void)API().HcnCloseEndpoint(p);
    }
};

using UniqueHcnEndpoint = std::unique_ptr<std::remove_pointer_t<HCN_ENDPOINT>, HcnEndpointCloser>;

struct CotaskmemStringDeleter
{
    void operator()(void* p) const noexcept
    {
        API().CoTaskMemFree(p);
    }
};
using UniqueCotaskmemString = std::unique_ptr<wchar_t, CotaskmemStringDeleter>;

namespace mpl = logging;
using lvl = mpl::Level;

// ---------------------------------------------------------

constexpr auto log_category = "HyperV-HCN-Wrapper";

// ---------------------------------------------------------

/**
 * Parse given GUID string into a GUID struct.
 *
 * @param guid_wstr GUID in wide string form, either 36 characters
 *                  (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 */
auto guid_from_string(const std::wstring& guid_wstr) -> ::GUID
{
    constexpr auto guid_length = 36;
    constexpr auto guid_length_with_braces = guid_length + 2;

    const auto input = [&guid_wstr]() {
        switch (guid_wstr.length())
        {
        case guid_length:
            // CLSIDFromString requires GUIDs to be wrapped with braces.
            return fmt::format(L"{{{}}}", guid_wstr);
        case guid_length_with_braces:
        {
            if (guid_wstr.front() != L'{' || guid_wstr.back() != L'}')
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
 *                 (without braces) or 38 characters (with braces.)
 *
 * @return GUID The parsed GUID
 */
auto guid_from_string(const std::string& guid_str) -> ::GUID
{
    const std::wstring v = to_wstring(guid_str);
    return guid_from_string(v);
}

// ---------------------------------------------------------

template <typename FnType>
OperationResult perform_hcn_operation(const FnType& fn)
{
    UniqueCotaskmemString result_msgbuf{};

    // Perform the operation. The last argument of the all HCN operations (except HcnClose*) is
    // ErrorRecord, which is a JSON-formatted document emitted by the API describing the error, if
    // it occurred. Therefore, we can streamline all API calls through perform_hcn_operation.
    const auto result = ResultCode{fn(out_ptr(result_msgbuf))};

    mpl::trace(log_category, "perform_hcn_operation(...) > result: {}", static_cast<bool>(result));

    // Avoid null to be forward-compatible with C++23
    return {result, {result_msgbuf ? result_msgbuf.get() : L""}};
}

// ---------------------------------------------------------

std::pair<OperationResult, UniqueHcnNetwork> open_network(const std::string& network_guid)
{
    mpl::trace(log_category, "open_network(...) > network_guid: {} ", network_guid);

    UniqueHcnNetwork network{};
    const auto result = perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnOpenNetwork(guid_from_string(network_guid), out_ptr(network), rmsgbuf);
    });

    return std::make_pair(result, std::move(network));
}

} // namespace

// ---------------------------------------------------------

HCNWrapper::HCNWrapper(const Singleton<HCNWrapper>::PrivatePass& pass) noexcept
    : Singleton<HCNWrapper>::Singleton{pass}
{
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_network(const CreateNetworkParameters& params) const
{
    mpl::trace(log_category, "HCNWrapper::create_network(...) > params: {} ", params);

    UniqueHcnNetwork network{};
    const auto network_settings = fmt::to_wstring(params);

    return perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnCreateNetwork(guid_from_string(params.guid),
                                      network_settings.c_str(),
                                      out_ptr(network),
                                      rmsgbuf);
    });
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_network(const std::string& network_guid) const
{
    mpl::trace(log_category, "HCNWrapper::delete_network(...) > network_guid: {}", network_guid);

    return perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnDeleteNetwork(guid_from_string(network_guid), rmsgbuf);
    });
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_endpoint(const CreateEndpointParameters& params) const
{
    mpl::trace(log_category, "HCNWrapper::create_endpoint(...) > params: {} ", params);

    const auto& [open_network_result, network] = open_network(params.network_guid);

    if (nullptr == network)
    {
        return open_network_result;
    }

    UniqueHcnEndpoint endpoint{};
    const auto params_json = fmt::to_wstring(params);

    return perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnCreateEndpoint(network.get(),
                                       guid_from_string(params.endpoint_guid),
                                       params_json.c_str(),
                                       out_ptr(endpoint),
                                       rmsgbuf);
    });
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_endpoint(const std::string& endpoint_guid) const
{
    mpl::trace(log_category,
               "HCNWrapper::delete_endpoint(...) > endpoint_guid: {} ",
               endpoint_guid);

    return perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnDeleteEndpoint(guid_from_string(endpoint_guid), rmsgbuf);
    });
}

// ---------------------------------------------------------

OperationResult HCNWrapper::enumerate_attached_endpoints(
    const std::string& vm_guid,
    std::vector<std::string>& endpoint_guids) const
{
    mpl::trace(log_category,
               "HCNWrapper::enumerate_attached_endpoints(...) > vm_guid: {} ",
               vm_guid);

    const auto query = EndpointQuery{.vm_guid = vm_guid};
    const auto query_wstring = fmt::to_wstring(query);

    UniqueCotaskmemString endpoints_json_output{}, result_msgbuf{};

    const auto result = API().HcnEnumerateEndpoints(query_wstring.c_str(),
                                                    out_ptr(endpoints_json_output),
                                                    out_ptr(result_msgbuf));

    if (endpoints_json_output)
    {
        const auto endpoints_as_str = wchar_to_utf8(endpoints_json_output.get());
        std::error_code ec;
        const auto as_json = boost::json::parse(endpoints_as_str, ec);
        if (!ec)
        {
            const auto& as_array = as_json.as_array();
            for (const auto& elem : as_array)
            {
                endpoint_guids.emplace_back(elem.as_string());
            }
        }
    }

    return {result, {result_msgbuf ? result_msgbuf.get() : L""}};
}

} // namespace multipass::hyperv::hcn
