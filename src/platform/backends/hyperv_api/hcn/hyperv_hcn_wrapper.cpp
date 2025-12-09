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
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/logging/log.h>
#include <multipass/utils.h>

// clang-format off
#include <windows.h>
#include <computecore.h>
#include <computedefs.h>
#include <computestorage.h>
#include <computenetwork.h>
#include <objbase.h>
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
    const std::wstring v = maybe_widen{guid_str};
    return guid_from_string(v);
}

// ---------------------------------------------------------

template <typename FnType>
OperationResult perform_hcn_operation(const FnType& fn)
{
    UniqueCotaskmemString result_msgbuf{};

    // Perform the operation. The last argument of the all HCN operations (except
    // HcnClose*) is ErrorRecord, which is a JSON-formatted document emitted by
    // the API describing the error, if it occurred. Therefore, we can streamline all
    // API calls through perform_hcn_operation.
    const auto result = ResultCode{fn(out_ptr(result_msgbuf))};

    mpl::trace(log_category, "perform_hcn_operation(...) > result: {}", static_cast<bool>(result));

    // Avoid null to be forward-compatible with C++23
    return {result, {result_msgbuf ? result_msgbuf.get() : L""}};
}

// ---------------------------------------------------------

UniqueHcnNetwork open_network(const std::string& network_guid)
{
    mpl::debug(log_category, "open_network(...) > network_guid: {} ", network_guid);

    UniqueHcnNetwork network{};
    const auto result = perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnOpenNetwork(guid_from_string(network_guid), out_ptr(network), rmsgbuf);
    });
    if (!result)
    {
        mpl::error(log_category, "open_network() > HcnOpenNetwork failed with {}!", result.code);
    }
    return network;
}

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

HCNWrapper::HCNWrapper(const Singleton<HCNWrapper>::PrivatePass& pass) noexcept
    : Singleton<HCNWrapper>::Singleton{pass}
{
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_network(const CreateNetworkParameters& params) const
{
    mpl::debug(log_category, "HCNWrapper::create_network(...) > params: {} ", params);

    UniqueHcnNetwork network{};
    const auto network_settings = fmt::to_wstring(params);

    const auto result = perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnCreateNetwork(guid_from_string(params.guid),
                                      network_settings.c_str(),
                                      out_ptr(network),
                                      rmsgbuf);
    });

    if (!result)
    {
        mpl::log(hcn_errc_to_log_level(result),
                 log_category,
                 "HCNWrapper::create_network(...) > HcnCreateNetwork failed with {}",
                 result.code);
    }

    return result;
}

// ---------------------------------------------------------

OperationResult HCNWrapper::delete_network(const std::string& network_guid) const
{
    mpl::debug(log_category, "HCNWrapper::delete_network(...) > network_guid: {}", network_guid);

    return perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnDeleteNetwork(guid_from_string(network_guid), rmsgbuf);
    });
}

// ---------------------------------------------------------

OperationResult HCNWrapper::create_endpoint(const CreateEndpointParameters& params) const
{
    mpl::debug(log_category, "HCNWrapper::create_endpoint(...) > params: {} ", params);

    const auto network = open_network(params.network_guid);

    if (nullptr == network)
    {
        return {E_POINTER, L"Could not open the network!"};
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
    mpl::debug(log_category,
               "HCNWrapper::delete_endpoint(...) > endpoint_guid: {} ",
               endpoint_guid);

    return perform_hcn_operation([&](auto&& rmsgbuf) {
        return API().HcnDeleteEndpoint(guid_from_string(endpoint_guid), rmsgbuf);
    });
}

} // namespace multipass::hyperv::hcn
