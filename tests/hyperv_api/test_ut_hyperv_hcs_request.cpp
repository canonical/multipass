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

#include "tests/common.h"
#include "tests/hyperv_api/hyperv_test_utils.h"

#include <hyperv_api/hcs/hyperv_hcs_request.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

using multipass::hyperv::universal_string_literal_helper;
using multipass::hyperv::hcs::HcsNetworkAdapter;
using multipass::hyperv::hcs::HcsRequest;
using multipass::hyperv::hcs::HcsRequestType;
using multipass::hyperv::hcs::HcsResourcePath;

namespace multipass::test
{

using uut_t = HcsRequest;

template <typename CharT>
struct HyperVHcsRequest_UnitTests : public ::testing::Test
{

    template <typename T>
    static std::basic_string<CharT> to_string(const T& v)
    {
        if constexpr (std::is_same_v<CharT, char>)
        {
            return fmt::to_string(v);
        }
        else if constexpr (std::is_same_v<CharT, wchar_t>)
        {
            return fmt::to_wstring(v);
        }
    }

    void do_test(const uut_t& uut, const universal_string_literal_helper& expected)
    {
        const auto result = to_string(uut);
        const std::basic_string<CharT> v{expected.as<CharT>()};
        const auto result_nws = trim_whitespace(result.c_str());
        const auto expected_nws = trim_whitespace(v.c_str());
        EXPECT_EQ(result_nws, expected_nws);
    }
};

using CharTypes = ::testing::Types<char, wchar_t>;
TYPED_TEST_SUITE(HyperVHcsRequest_UnitTests, CharTypes);

// ---------------------------------------------------------

TYPED_TEST(HyperVHcsRequest_UnitTests, network_adapter_add_no_settings)
{
    uut_t uut{HcsResourcePath::NetworkAdapters("1111-2222-3333"), HcsRequestType::Add()};

    constexpr auto expected_result = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{1111-2222-3333}",
            "RequestType": "Add",
            "Settings": null
        })json");

    TestFixture::do_test(uut, expected_result);
}

// ---------------------------------------------------------

TYPED_TEST(HyperVHcsRequest_UnitTests, network_adapter_remove)
{
    uut_t uut{HcsResourcePath::NetworkAdapters("1111-2222-3333"), HcsRequestType::Remove()};

    constexpr auto expected_result = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{1111-2222-3333}",
            "RequestType": "Remove",
            "Settings": null
        })json");

    TestFixture::do_test(uut, expected_result);
}

// ---------------------------------------------------------

TYPED_TEST(HyperVHcsRequest_UnitTests, network_adapter_add_with_settings)
{
    uut_t uut{HcsResourcePath::NetworkAdapters("1111-2222-3333"), HcsRequestType::Add()};
    hyperv::hcs::HcsNetworkAdapter settings{};
    settings.endpoint_guid = "endpoint guid";
    settings.mac_address = "mac address";
    settings.instance_guid = "instance guid";
    uut.settings = settings;
    constexpr auto expected_result = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        {
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{1111-2222-3333}",
            "RequestType": "Add",
            "Settings": {
                "EndpointId": "endpoint guid",
                "MacAddress": "mac address",
                "InstanceId": "endpoint guid"
            }
        })json");

    TestFixture::do_test(uut, expected_result);
}

} // namespace multipass::test
