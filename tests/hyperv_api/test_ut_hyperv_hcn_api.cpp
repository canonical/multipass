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

#include "hyperv_test_utils.h"
#include "tests/common.h"
#include "tests/hyperv_api/mock_hyperv_hcn_api.h"
#include "tests/mock_logger.h"

#include <hyperv_api/hcn/hyperv_hcn_api.h>
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>

#include <shared/windows/guid_formatter.h>

#include <multipass/logging/level.h>

#include <combaseapi.h>
#include <computenetwork.h>
#include <winerror.h>

namespace mpt = multipass::test;
namespace mpl = multipass::logging;
namespace hcn = multipass::hyperv::hcn;

using testing::DoAll;
using testing::Return;
using testing::StrictMock;

namespace multipass::test
{

using hcn::HCN;

struct HyperVHCNAPI_UnitTests : public ::testing::Test
{

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    mpt::MockHCNAPI::GuardedMock mock_hcn_api_injection = mpt::MockHCNAPI::inject<StrictMock>();
    mpt::MockHCNAPI& mock_hcn_api = *mock_hcn_api_injection.first;

    // Sentinel values as mock API parameters. These handles are opaque handles and
    // they're not being dereferenced in any way -- only address values are compared.
    inline static auto mock_network_object = reinterpret_cast<HCN_NETWORK>(0xbadf00d);
    inline static auto mock_endpoint_object = reinterpret_cast<HCN_ENDPOINT>(0xbadcafe);

    // Generic error message for all tests, intended to be used for API calls returning
    // an "error_record".
    inline static wchar_t mock_error_msg[16] = L"It's a failure.";
};

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_network_success_ics)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    constexpr auto expected_network_settings = LR"""(
                    {
                        "SchemaVersion": {
                            "Major": 2,
                            "Minor": 2
                        },
                        "Name": "multipass-hyperv-api-hcn-create-test",
                        "Type": "ICS",
                        "Ipams": [
                            {
                                "Type": "static",
                                "Subnets": [
                                    {
                                        "Policies": [],
                                        "Routes": [],
                                        "IpAddressPrefix": "172.50.224.0/20",
                                        "IpSubnets": null
                                    }
                                ]
                            }
                        ],
                        "Flags" : 0,
                        "Policies": []
                    }
                    )""";
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    const auto config_no_whitespace = trim_whitespace(settings);
                    const auto expected_no_whitespace = trim_whitespace(expected_network_settings);
                    ASSERT_STREQ(config_no_whitespace.c_str(), expected_no_whitespace.c_str());
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", fmt::to_string(id));
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateNetworkParameters params{};
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.ipams = {
            hcn::HcnIpam{hcn::HcnIpamType::Static(), {hcn::HcnSubnet{"172.50.224.0/20"}}}};

        const auto& [status, status_msg] = HCN().create_network(params);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_network_success_transparent)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    constexpr auto expected_network_settings = LR"""(
                    {
                        "SchemaVersion": {
                            "Major": 2,
                            "Minor": 2
                        },
                        "Name": "multipass-hyperv-api-hcn-create-test",
                        "Type": "Transparent",
                        "Ipams": [
                        ],
                        "Flags" : 0,
                        "Policies": [
                         {
                            "Type": "NetAdapterName",
                            "Settings":
                            {
                                "NetworkAdapterName": "test adapter"
                            }
                        }
                        ]
                    }
                    )""";
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    const auto config_no_whitespace = trim_whitespace(settings);
                    const auto expected_no_whitespace = trim_whitespace(expected_network_settings);
                    ASSERT_STREQ(config_no_whitespace.c_str(), expected_no_whitespace.c_str());
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", fmt::to_string(id));
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateNetworkParameters params{};
        params.type = hcn::HcnNetworkType::Transparent();
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.ipams = {};
        hcn::HcnNetworkPolicy policy{hcn::HcnNetworkPolicyType::NetAdapterName(),
                                     hcn::HcnNetworkPolicyNetAdapterName{"test adapter"}};
        params.policies.push_back(policy);

        const auto& [status, status_msg] = HCN().create_network(params);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_network_success_with_flags_multiple_policies)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    constexpr auto expected_network_settings = LR"""(
                    {
                        "SchemaVersion": {
                            "Major": 2,
                            "Minor": 2
                        },
                        "Name": "multipass-hyperv-api-hcn-create-test",
                        "Type": "Transparent",
                        "Ipams": [
                        ],
                        "Flags" : 10,
                        "Policies": [
                         {
                            "Type": "NetAdapterName",
                            "Settings":
                            {
                                "NetworkAdapterName": "test adapter"
                            }
                        },
                        {
                            "Type": "NetAdapterName",
                            "Settings":
                            {
                                "NetworkAdapterName": "test adapter"
                            }
                        }
                        ]
                    }
                    )""";
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    const auto config_no_whitespace = trim_whitespace(settings);
                    const auto expected_no_whitespace = trim_whitespace(expected_network_settings);
                    ASSERT_STREQ(config_no_whitespace.c_str(), expected_no_whitespace.c_str());
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", fmt::to_string(id));
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateNetworkParameters params{};
        params.type = hcn::HcnNetworkType::Transparent();
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.ipams = {};
        params.flags =
            hcn::HcnNetworkFlags::enable_dhcp_server | hcn::HcnNetworkFlags::enable_non_persistent;
        hcn::HcnNetworkPolicy policy{hcn::HcnNetworkPolicyType::NetAdapterName(),
                                     hcn::HcnNetworkPolicyNetAdapterName{"test adapter"}};
        params.policies.push_back(policy);
        params.policies.push_back(policy);

        const auto& [status, status_msg] = HCN().create_network(params);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_network_success_multiple_ipams)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    constexpr auto expected_network_settings = LR"""(
                    {
                        "SchemaVersion": {
                            "Major": 2,
                            "Minor": 2
                        },
                        "Name": "multipass-hyperv-api-hcn-create-test",
                        "Type": "Transparent",
                        "Ipams": [
                            {
                                "Type": "static",
                                "Subnets": [
                                    {
                                        "Policies": [],
                                        "Routes": [
                                            {
                                                "NextHop": "10.0.0.1",
                                                "DestinationPrefix": "0.0.0.0/0",
                                                "Metric": 0
                                            }
                                        ],
                                        "IpAddressPrefix": "10.0.0.10/10",
                                        "IpSubnets": null
                                    }
                                ]
                            },
                             {
                                "Type": "DHCP",
                                "Subnets": []
                            }
                        ],
                        "Flags" : 0,
                        "Policies": []
                    }
                    )""";
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    const auto config_no_whitespace = trim_whitespace(settings);
                    const auto expected_no_whitespace = trim_whitespace(expected_network_settings);
                    ASSERT_STREQ(config_no_whitespace.c_str(), expected_no_whitespace.c_str());
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", fmt::to_string(id));
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateNetworkParameters params{};
        params.type = hcn::HcnNetworkType::Transparent();
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        hcn::HcnIpam ipam1;
        ipam1.type = hcn::HcnIpamType::Static();
        ipam1.subnets.push_back(
            hcn::HcnSubnet{"10.0.0.10/10", {hcn::HcnRoute{"10.0.0.1", "0.0.0.0/0", 0}}});
        hcn::HcnIpam ipam2;
        ipam2.type = hcn::HcnIpamType::Dhcp();

        params.ipams.push_back(ipam1);
        params.ipams.push_back(ipam2);

        const auto& [status, status_msg] = HCN().create_network(params);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Success scenario 2: HcnCloseNetwork returns an error.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_network_close_network_failed)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); },
                            Return(E_POINTER)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCNWrapper::create_network(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_hcn_operation(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateNetworkParameters params{};
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.ipams = {
            hcn::HcnIpam{hcn::HcnIpamType::Static(), {hcn::HcnSubnet{"172.50.224.0/20"}}}};

        const auto& [success, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Failure scenario 1: HcnCreateNetwork returns an error.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_network_failed)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    *network = mock_network_object;
                    *error_record = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, CoTaskMemFree).WillOnce([&](void* ptr) {
            EXPECT_EQ(ptr, mock_error_msg);
        });

        logger_scope.mock_logger->expect_log(mpl::Level::debug, "HCNWrapper::create_network(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_hcn_operation(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateNetworkParameters params{};
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.ipams = {
            hcn::HcnIpam{hcn::HcnIpamType::Static(), {hcn::HcnSubnet{"172.50.224.0/20"}}}};

        const auto& [success, error_msg] = HCN().create_network(params);
        ASSERT_FALSE(success);
        ASSERT_EQ(static_cast<HRESULT>(success), E_POINTER);
        ASSERT_FALSE(error_msg.empty());
        ASSERT_STREQ(error_msg.c_str(), mock_error_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, delete_network_success)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnDeleteNetwork)
            .WillOnce(DoAll(
                [&](REFGUID guid, PWSTR* error_record) {
                    ASSERT_EQ("af3fb745-2f23-463c-8ded-443f876d9e81", fmt::to_string(guid));
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, error_record);
                },
                Return(NOERROR)));

        // Expected logs
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "HCNWrapper::delete_network(...) > network_guid: af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: true");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        const auto& [status, error_msg] =
            HCN().delete_network("af3fb745-2f23-463c-8ded-443f876d9e81");
        ASSERT_TRUE(status);
        ASSERT_TRUE(error_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Failure scenario: API call returns non-success
 */
TEST_F(HyperVHCNAPI_UnitTests, delete_network_failed)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnDeleteNetwork)
            .WillOnce(DoAll(
                [&](REFGUID, PWSTR* error_record) {
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, error_record);
                    *error_record = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_hcn_api, CoTaskMemFree).WillOnce([&](void* ptr) {
            EXPECT_EQ(ptr, mock_error_msg);
        });
        // Expected logs
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "HCNWrapper::delete_network(...) > network_guid: af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        const auto& [status, error_msg] =
            HCN().delete_network("af3fb745-2f23-463c-8ded-443f876d9e81");
        ASSERT_FALSE(status);
        ASSERT_FALSE(error_msg.empty());
        ASSERT_STREQ(error_msg.c_str(), mock_error_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_endpoint_success)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateEndpoint)
            .WillOnce(DoAll(
                [&](HCN_NETWORK network,
                    REFGUID id,
                    PCWSTR settings,
                    PHCN_ENDPOINT endpoint,
                    PWSTR* error_record) {
                    constexpr auto expected_endpoint_settings = LR"""(
                    {
                        "SchemaVersion": {
                            "Major": 2,
                            "Minor": 16
                        },
                        "HostComputeNetwork": "b70c479d-f808-4053-aafa-705bc15b6d68",
                        "Policies": [
                        ],
                        "MacAddress": null
                    })""";

                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(mock_network_object, network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, endpoint);
                    ASSERT_EQ(nullptr, *endpoint);
                    const auto config_no_whitespace = trim_whitespace(settings);
                    const auto expected_no_whitespace = trim_whitespace(expected_endpoint_settings);
                    ASSERT_STREQ(config_no_whitespace.c_str(), expected_no_whitespace.c_str());
                    ASSERT_EQ("77c27c1e-8204-437d-a7cc-fb4ce1614819", fmt::to_string(id));
                    *endpoint = mock_endpoint_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseEndpoint)
            .WillOnce(DoAll([&](HCN_ENDPOINT n) { ASSERT_EQ(n, mock_endpoint_object); },
                            Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnOpenNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PHCN_NETWORK network, PWSTR* error_record) {
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", fmt::to_string(id));
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));

        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "HCNWrapper::create_endpoint(...) > params: ");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "open_network(...) > network_guid: b70c479d-f808-4053-aafa-705bc15b6d68");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: true",
                                             testing::Exactly(2));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateEndpointParameters params{};
        params.endpoint_guid = "77c27c1e-8204-437d-a7cc-fb4ce1614819";
        params.network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";

        const auto& [success, error_msg] = HCN().create_endpoint(params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Failure scenario: internal open_network call fails.
 */
TEST_F(HyperVHCNAPI_UnitTests, create_endpoint_open_network_failed)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnOpenNetwork).WillOnce(Return(E_POINTER));

        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "HCNWrapper::create_endpoint(...) > params: ");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "open_network(...) > network_guid: b70c479d-f808-4053-aafa-705bc15b6d68");
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "open_network() > HcnOpenNetwork failed with 0x80004003");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateEndpointParameters params{};
        params.endpoint_guid = "77c27c1e-8204-437d-a7cc-fb4ce1614819";
        params.network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";

        const auto& [status, error_msg] = HCN().create_endpoint(params);
        ASSERT_FALSE(status);
        ASSERT_EQ(E_POINTER, static_cast<HRESULT>(status));
        ASSERT_FALSE(error_msg.empty());
        ASSERT_STREQ(error_msg.c_str(), L"Could not open the network!");
    }
}

// ---------------------------------------------------------

TEST_F(HyperVHCNAPI_UnitTests, create_endpoint_failure)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnCreateEndpoint)
            .WillOnce(DoAll(
                [&](HCN_NETWORK network,
                    REFGUID id,
                    PCWSTR settings,
                    PHCN_ENDPOINT endpoint,
                    PWSTR* error_record) {
                    constexpr auto expected_endpoint_settings = LR"""(
                    {
                        "SchemaVersion": {
                            "Major": 2,
                            "Minor": 16
                        },
                        "HostComputeNetwork": "b70c479d-f808-4053-aafa-705bc15b6d68",
                        "Policies": [
                        ],
                        "MacAddress": null
                    })""";

                    ASSERT_EQ(mock_network_object, network);
                    ASSERT_NE(nullptr, error_record);
                    const auto config_no_whitespace = trim_whitespace(settings);
                    const auto expected_no_whitespace = trim_whitespace(expected_endpoint_settings);
                    ASSERT_STREQ(config_no_whitespace.c_str(), expected_no_whitespace.c_str());
                    ASSERT_EQ("77c27c1e-8204-437d-a7cc-fb4ce1614819", fmt::to_string(id));
                    *endpoint = mock_endpoint_object;
                    *error_record = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_hcn_api, HcnCloseEndpoint)
            .WillOnce(DoAll([&](HCN_ENDPOINT n) { ASSERT_EQ(n, mock_endpoint_object); },
                            Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnOpenNetwork)
            .WillOnce(DoAll(
                [&](REFGUID id, PHCN_NETWORK network, PWSTR* error_record) {
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", fmt::to_string(id));
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, HcnCloseNetwork)
            .WillOnce(
                DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));

        EXPECT_CALL(mock_hcn_api, CoTaskMemFree).WillOnce([](const void* ptr) {
            ASSERT_EQ(ptr, mock_error_msg);
        });

        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "HCNWrapper::create_endpoint(...) > params: ");
        logger_scope.mock_logger->expect_log(
            mpl::Level::debug,
            "open_network(...) > network_guid: b70c479d-f808-4053-aafa-705bc15b6d68");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: true");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hcn::CreateEndpointParameters params{};
        params.endpoint_guid = "77c27c1e-8204-437d-a7cc-fb4ce1614819";
        params.network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";

        const auto& [success, error_msg] = HCN().create_endpoint(params);
        ASSERT_FALSE(success);
        ASSERT_FALSE(error_msg.empty());
        ASSERT_STREQ(error_msg.c_str(), mock_error_msg);
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, delete_endpoint_success)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnDeleteEndpoint)
            .WillOnce(DoAll(
                [&](REFGUID guid, PWSTR* error_record) {
                    ASSERT_EQ("af3fb745-2f23-463c-8ded-443f876d9e81", fmt::to_string(guid));
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, error_record);
                },
                Return(NOERROR)));

        // Expected logs
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "HCNWrapper::delete_endpoint(...) > endpoint_guid: "
                                             "af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: true");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        const auto& [status, error_msg] =
            HCN().delete_endpoint("af3fb745-2f23-463c-8ded-443f876d9e81");
        ASSERT_TRUE(status);
        ASSERT_TRUE(error_msg.empty());
    }
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNAPI_UnitTests, delete_endpoint_failure)
{
    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_hcn_api, HcnDeleteEndpoint)
            .WillOnce(DoAll([&](REFGUID, PWSTR* error_record) { *error_record = mock_error_msg; },
                            Return(E_POINTER)));

        EXPECT_CALL(mock_hcn_api, CoTaskMemFree).WillOnce([](const void* ptr) {
            ASSERT_EQ(ptr, mock_error_msg);
        });

        // Expected logs
        logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                             "HCNWrapper::delete_endpoint(...) > endpoint_guid: "
                                             "af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_hcn_operation(...) > result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        const auto& [status, error_msg] =
            HCN().delete_endpoint("af3fb745-2f23-463c-8ded-443f876d9e81");
        ASSERT_FALSE(status);
        ASSERT_FALSE(error_msg.empty());
        ASSERT_STREQ(error_msg.c_str(), mock_error_msg);
    }
}

} // namespace multipass::test
