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

#include "hyperv_api/hcn/hyperv_hcn_api_table.h"
#include "tests/mock_logger.h"

#include "gmock/gmock.h"
#include <combaseapi.h>
#include <computenetwork.h>
#include <multipass/logging/level.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_api_wrapper.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hyperv_api_common.h>
#include <winerror.h>

namespace mpt = multipass::test;
namespace mpl = multipass::logging;

using testing::DoAll;
using testing::Return;

#define EXPECT_NO_CALL(mock) EXPECT_CALL(mock, Call).Times(0)

namespace multipass::test
{

using uut_t = hyperv::hcn::HCNWrapper;

struct HyperVHCNAPI_UnitTests : public ::testing::Test
{

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    void SetUp() override
    {

        // Each of the unit tests are expected to have their own mock functions
        // and override the mock_api_table with them. Hence, the stub mocks should
        // not be called at all.
        // If any of them do get called, then:
        //
        // a-) You have forgotten to mock something
        // b-) The implementation is using a function that you didn't expect
        //
        // Either way, you should have a look.

        EXPECT_NO_CALL(stub_mock_create_network);
        EXPECT_NO_CALL(stub_mock_open_network);
        EXPECT_NO_CALL(stub_mock_delete_network);
        EXPECT_NO_CALL(stub_mock_close_network);
        EXPECT_NO_CALL(stub_mock_create_endpoint);
        EXPECT_NO_CALL(stub_mock_open_endpoint);
        EXPECT_NO_CALL(stub_mock_delete_endpoint);
        EXPECT_NO_CALL(stub_mock_close_endpoint);
        EXPECT_NO_CALL(stub_mock_cotaskmemfree);
    }

    void TearDown() override
    {
    }

    // Set of placeholder mocks in order to catch *unexpected* calls.
    ::testing::MockFunction<decltype(HcnCreateNetwork)> stub_mock_create_network;
    ::testing::MockFunction<decltype(HcnOpenNetwork)> stub_mock_open_network;
    ::testing::MockFunction<decltype(HcnDeleteNetwork)> stub_mock_delete_network;
    ::testing::MockFunction<decltype(HcnCloseNetwork)> stub_mock_close_network;
    ::testing::MockFunction<decltype(HcnCreateEndpoint)> stub_mock_create_endpoint;
    ::testing::MockFunction<decltype(HcnOpenEndpoint)> stub_mock_open_endpoint;
    ::testing::MockFunction<decltype(HcnDeleteEndpoint)> stub_mock_delete_endpoint;
    ::testing::MockFunction<decltype(HcnCloseEndpoint)> stub_mock_close_endpoint;
    ::testing::MockFunction<decltype(::CoTaskMemFree)> stub_mock_cotaskmemfree;

    // Initialize the API table with stub functions, so if any of these fire without
    // our will, we'll know.
    hyperv::hcn::HCNAPITable mock_api_table{stub_mock_create_network.AsStdFunction(),
                                            stub_mock_open_network.AsStdFunction(),
                                            stub_mock_delete_network.AsStdFunction(),
                                            stub_mock_close_network.AsStdFunction(),
                                            stub_mock_create_endpoint.AsStdFunction(),
                                            stub_mock_open_endpoint.AsStdFunction(),
                                            stub_mock_delete_endpoint.AsStdFunction(),
                                            stub_mock_close_endpoint.AsStdFunction(),
                                            stub_mock_cotaskmemfree.AsStdFunction()};

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
TEST_F(HyperVHCNAPI_UnitTests, create_network_success)
{
    /******************************************************
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnCreateNetwork)> mock_create_network;
    ::testing::MockFunction<decltype(HcnCloseNetwork)> mock_close_network;

    mock_api_table.CreateNetwork = mock_create_network.AsStdFunction();
    mock_api_table.CloseNetwork = mock_close_network.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    constexpr auto expected_network_settings = LR"""(
{
    "Name": "multipass-hyperv-api-hcn-create-test",
    "Type": "ICS",
    "Subnets" : [
        {
            "GatewayAddress": "172.50.224.1",
            "AddressPrefix" : "172.50.224.0/20",
            "IpSubnets" : [
                {
                    "IpAddressPrefix": "172.50.224.0/20"
                }
            ]
        }
    ],
    "IsolateSwitch": true,
    "Flags" : 265
}
)""";
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_STREQ(settings, expected_network_settings);
                    const auto guid_str = hyperv::guid_to_string(id);
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", guid_str);
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_network, Call)
            .WillOnce(DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcn::CreateNetworkParameters params{};
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.subnet = "172.50.224.0/20";
        params.gateway = "172.50.224.1";

        const auto& [status, status_msg] = uut.create_network(params);
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnCreateNetwork)> mock_create_network;
    ::testing::MockFunction<decltype(HcnCloseNetwork)> mock_close_network;

    mock_api_table.CreateNetwork = mock_create_network.AsStdFunction();
    mock_api_table.CloseNetwork = mock_close_network.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_network, Call)
            .WillOnce(DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(E_POINTER)));

        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::create_network(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...)");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hyperv::hcn::CreateNetworkParameters params{};
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.subnet = "172.50.224.0/20";
        params.gateway = "172.50.224.1";

        uut_t uut{mock_api_table};
        const auto& [success, error_msg] = uut.create_network(params);
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnCreateNetwork)> mock_create_network;
    ::testing::MockFunction<decltype(HcnCloseNetwork)> mock_close_network;
    ::testing::MockFunction<decltype(::CoTaskMemFree)> mock_cotaskmemfree;

    mock_api_table.CreateNetwork = mock_create_network.AsStdFunction();
    mock_api_table.CloseNetwork = mock_close_network.AsStdFunction();
    mock_api_table.CoTaskMemFree = mock_cotaskmemfree.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID id, PCWSTR settings, PHCN_NETWORK network, PWSTR* error_record) {
                    *network = mock_network_object;
                    *error_record = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_close_network, Call)
            .WillOnce(DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));

        EXPECT_CALL(mock_cotaskmemfree, Call).WillOnce([&](void* ptr) { EXPECT_EQ(ptr, mock_error_msg); });

        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::create_network(...)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::error,
            "HCNWrapper::create_network(...) > HcnCreateNetwork failed with 0x80004003!");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        hyperv::hcn::CreateNetworkParameters params{};
        params.name = "multipass-hyperv-api-hcn-create-test";
        params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
        params.subnet = "172.50.224.0/20";
        params.gateway = "172.50.224.1";

        uut_t uut{mock_api_table};
        const auto& [success, error_msg] = uut.create_network(params);
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnDeleteNetwork)> mock_delete_network;

    mock_api_table.DeleteNetwork = mock_delete_network.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_delete_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID guid, PWSTR* error_record) {
                    const auto guid_str = hyperv::guid_to_string(guid);
                    ASSERT_EQ("af3fb745-2f23-463c-8ded-443f876d9e81", guid_str);
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, error_record);
                },
                Return(NOERROR)));

        // Expected logs
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::delete_network(...) > network_guid: af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: true");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        const auto& [status, error_msg] = uut.delete_network("af3fb745-2f23-463c-8ded-443f876d9e81");
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnDeleteNetwork)> mock_delete_network;
    ::testing::MockFunction<decltype(::CoTaskMemFree)> mock_cotaskmemfree;

    mock_api_table.DeleteNetwork = mock_delete_network.AsStdFunction();
    mock_api_table.CoTaskMemFree = mock_cotaskmemfree.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_delete_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID, PWSTR* error_record) {
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, error_record);
                    *error_record = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_cotaskmemfree, Call).WillOnce([&](void* ptr) { EXPECT_EQ(ptr, mock_error_msg); });
        // Expected logs
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::delete_network(...) > network_guid: af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        const auto& [status, error_msg] = uut.delete_network("af3fb745-2f23-463c-8ded-443f876d9e81");
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnCreateEndpoint)> mock_create_endpoint;
    ::testing::MockFunction<decltype(HcnCloseEndpoint)> mock_close_endpoint;
    ::testing::MockFunction<decltype(HcnOpenNetwork)> mock_open_network;
    ::testing::MockFunction<decltype(HcnCloseNetwork)> mock_close_network;

    mock_api_table.CreateEndpoint = mock_create_endpoint.AsStdFunction();
    mock_api_table.CloseEndpoint = mock_close_endpoint.AsStdFunction();
    mock_api_table.OpenNetwork = mock_open_network.AsStdFunction();
    mock_api_table.CloseNetwork = mock_close_network.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_endpoint, Call)
            .WillOnce(DoAll(
                [&](HCN_NETWORK network, REFGUID id, PCWSTR settings, PHCN_ENDPOINT endpoint, PWSTR* error_record) {
                    constexpr auto expected_endpoint_settings = LR"""(
{
    "SchemaVersion": {
        "Major": 2,
        "Minor": 16
    },
    "HostComputeNetwork": "b70c479d-f808-4053-aafa-705bc15b6d68",
    "Policies": [
    ],
    "IpConfigurations": [
        {
            "IpAddress": "172.50.224.27"
        }
    ]
})""";

                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(mock_network_object, network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, endpoint);
                    ASSERT_EQ(nullptr, *endpoint);
                    ASSERT_STREQ(settings, expected_endpoint_settings);
                    const auto endpoint_guid_str = hyperv::guid_to_string(id);
                    ASSERT_EQ("77c27c1e-8204-437d-a7cc-fb4ce1614819", endpoint_guid_str);
                    *endpoint = mock_endpoint_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_endpoint, Call)
            .WillOnce(DoAll([&](HCN_ENDPOINT n) { ASSERT_EQ(n, mock_endpoint_object); }, Return(NOERROR)));

        EXPECT_CALL(mock_open_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID id, PHCN_NETWORK network, PWSTR* error_record) {
                    const auto expected_network_guid_str = hyperv::guid_to_string(id);
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", expected_network_guid_str);
                    ASSERT_NE(nullptr, network);
                    ASSERT_EQ(nullptr, *network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_network, Call)
            .WillOnce(DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));

        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::create_endpoint(...) > params: Endpoint GUID: (77c27c1e-8204-437d-a7cc-fb4ce1614819) | "
            "Network GUID: (b70c479d-f808-4053-aafa-705bc15b6d68) | Endpoint IPvX Addr.: (172.50.224.27)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "open_network(...) > network_guid: b70c479d-f808-4053-aafa-705bc15b6d68");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "perform_operation(...) > fn: 0x0, result: true",
                                             testing::Exactly(2));
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcn::CreateEndpointParameters params{};
        params.endpoint_guid = "77c27c1e-8204-437d-a7cc-fb4ce1614819";
        params.network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
        params.endpoint_ipvx_addr = "172.50.224.27";

        const auto& [success, error_msg] = uut.create_endpoint(params);
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnOpenNetwork)> mock_open_network;

    mock_api_table.OpenNetwork = mock_open_network.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_open_network, Call).WillOnce(Return(E_POINTER));

        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::create_endpoint(...) > params: Endpoint GUID: (77c27c1e-8204-437d-a7cc-fb4ce1614819) | "
            "Network GUID: (b70c479d-f808-4053-aafa-705bc15b6d68) | Endpoint IPvX Addr.: (172.50.224.27)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "open_network(...) > network_guid: b70c479d-f808-4053-aafa-705bc15b6d68");
        logger_scope.mock_logger->expect_log(mpl::Level::error,
                                             "open_network() > HcnOpenNetwork failed with 0x80004003!");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcn::CreateEndpointParameters params{};
        params.endpoint_guid = "77c27c1e-8204-437d-a7cc-fb4ce1614819";
        params.network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
        params.endpoint_ipvx_addr = "172.50.224.27";

        const auto& [status, error_msg] = uut.create_endpoint(params);
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
     * Override the default mock functions.
     ******************************************************/

    ::testing::MockFunction<decltype(HcnCreateEndpoint)> mock_create_endpoint;
    ::testing::MockFunction<decltype(HcnCloseEndpoint)> mock_close_endpoint;
    ::testing::MockFunction<decltype(HcnOpenNetwork)> mock_open_network;
    ::testing::MockFunction<decltype(HcnCloseNetwork)> mock_close_network;
    ::testing::MockFunction<decltype(CoTaskMemFree)> mock_cotaskmemfree;

    mock_api_table.CreateEndpoint = mock_create_endpoint.AsStdFunction();
    mock_api_table.CloseEndpoint = mock_close_endpoint.AsStdFunction();
    mock_api_table.OpenNetwork = mock_open_network.AsStdFunction();
    mock_api_table.CloseNetwork = mock_close_network.AsStdFunction();
    mock_api_table.CoTaskMemFree = mock_cotaskmemfree.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_create_endpoint, Call)
            .WillOnce(DoAll(
                [&](HCN_NETWORK network, REFGUID id, PCWSTR settings, PHCN_ENDPOINT endpoint, PWSTR* error_record) {
                    constexpr auto expected_endpoint_settings = LR"""(
{
    "SchemaVersion": {
        "Major": 2,
        "Minor": 16
    },
    "HostComputeNetwork": "b70c479d-f808-4053-aafa-705bc15b6d68",
    "Policies": [
    ],
    "IpConfigurations": [
        {
            "IpAddress": "172.50.224.27"
        }
    ]
})""";

                    ASSERT_EQ(mock_network_object, network);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_STREQ(settings, expected_endpoint_settings);
                    const auto expected_endpoint_guid_str = hyperv::guid_to_string(id);
                    ASSERT_EQ("77c27c1e-8204-437d-a7cc-fb4ce1614819", expected_endpoint_guid_str);
                    *endpoint = mock_endpoint_object;
                    *error_record = mock_error_msg;
                },
                Return(E_POINTER)));

        EXPECT_CALL(mock_close_endpoint, Call)
            .WillOnce(DoAll([&](HCN_ENDPOINT n) { ASSERT_EQ(n, mock_endpoint_object); }, Return(NOERROR)));

        EXPECT_CALL(mock_open_network, Call)
            .WillOnce(DoAll(
                [&](REFGUID id, PHCN_NETWORK network, PWSTR* error_record) {
                    const auto expected_network_guid_str = hyperv::guid_to_string(id);
                    ASSERT_EQ("b70c479d-f808-4053-aafa-705bc15b6d68", expected_network_guid_str);
                    ASSERT_NE(nullptr, error_record);
                    ASSERT_EQ(nullptr, *error_record);
                    *network = mock_network_object;
                },
                Return(NOERROR)));

        EXPECT_CALL(mock_close_network, Call)
            .WillOnce(DoAll([&](HCN_NETWORK n) { ASSERT_EQ(n, mock_network_object); }, Return(NOERROR)));

        EXPECT_CALL(mock_cotaskmemfree, Call).WillOnce([](const void* ptr) { ASSERT_EQ(ptr, mock_error_msg); });

        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::create_endpoint(...) > params: Endpoint GUID: (77c27c1e-8204-437d-a7cc-fb4ce1614819) | "
            "Network GUID: (b70c479d-f808-4053-aafa-705bc15b6d68) | Endpoint IPvX Addr.: (172.50.224.27)");
        logger_scope.mock_logger->expect_log(mpl::Level::trace,
                                             "open_network(...) > network_guid: b70c479d-f808-4053-aafa-705bc15b6d68");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: true");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        hyperv::hcn::CreateEndpointParameters params{};
        params.endpoint_guid = "77c27c1e-8204-437d-a7cc-fb4ce1614819";
        params.network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
        params.endpoint_ipvx_addr = "172.50.224.27";

        const auto& [success, error_msg] = uut.create_endpoint(params);
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnDeleteEndpoint)> mock_delete_endpoint;

    mock_api_table.DeleteEndpoint = mock_delete_endpoint.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_delete_endpoint, Call)
            .WillOnce(DoAll(
                [&](REFGUID guid, PWSTR* error_record) {
                    const auto guid_str = hyperv::guid_to_string(guid);
                    ASSERT_EQ("af3fb745-2f23-463c-8ded-443f876d9e81", guid_str);
                    ASSERT_EQ(nullptr, *error_record);
                    ASSERT_NE(nullptr, error_record);
                },
                Return(NOERROR)));

        // Expected logs
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::delete_endpoint(...) > endpoint_guid: af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: true");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        const auto& [status, error_msg] = uut.delete_endpoint("af3fb745-2f23-463c-8ded-443f876d9e81");
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
     * Override the default mock functions.
     ******************************************************/
    ::testing::MockFunction<decltype(HcnDeleteEndpoint)> mock_delete_endpoint;
    ::testing::MockFunction<decltype(::CoTaskMemFree)> mock_cotaskmemfree;

    mock_api_table.DeleteEndpoint = mock_delete_endpoint.AsStdFunction();
    mock_api_table.CoTaskMemFree = mock_cotaskmemfree.AsStdFunction();

    /******************************************************
     * Verify that the dependencies are called with right
     * data.
     ******************************************************/
    {
        EXPECT_CALL(mock_delete_endpoint, Call)
            .WillOnce(DoAll([&](REFGUID, PWSTR* error_record) { *error_record = mock_error_msg; }, Return(E_POINTER)));

        EXPECT_CALL(mock_cotaskmemfree, Call).WillOnce([](const void* ptr) { ASSERT_EQ(ptr, mock_error_msg); });

        // Expected logs
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "HCNWrapper::HCNWrapper(...)");
        logger_scope.mock_logger->expect_log(
            mpl::Level::trace,
            "HCNWrapper::delete_endpoint(...) > endpoint_guid: af3fb745-2f23-463c-8ded-443f876d9e81");
        logger_scope.mock_logger->expect_log(mpl::Level::trace, "perform_operation(...) > fn: 0x0, result: false");
    }

    /******************************************************
     * Verify the expected outcome.
     ******************************************************/
    {
        uut_t uut{mock_api_table};
        const auto& [status, error_msg] = uut.delete_endpoint("af3fb745-2f23-463c-8ded-443f876d9e81");
        ASSERT_FALSE(status);
        ASSERT_FALSE(error_msg.empty());
        ASSERT_STREQ(error_msg.c_str(), mock_error_msg);
    }
}

} // namespace multipass::test
