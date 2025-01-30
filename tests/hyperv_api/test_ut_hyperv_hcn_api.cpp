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
#include "tests/common.h"
#include "tests/mock_logger.h"

#include "gmock/gmock.h"
#include <computenetwork.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_api_wrapper.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>

namespace mpt = multipass::test;

using testing::_;
using testing::DoAll;
using testing::Invoke;
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

    hyperv::hcn::HCNAPITable mock_api_table{stub_mock_create_network.AsStdFunction(),
                                            stub_mock_open_network.AsStdFunction(),
                                            stub_mock_delete_network.AsStdFunction(),
                                            stub_mock_close_network.AsStdFunction(),
                                            stub_mock_create_endpoint.AsStdFunction(),
                                            stub_mock_open_endpoint.AsStdFunction(),
                                            stub_mock_delete_endpoint.AsStdFunction(),
                                            stub_mock_close_endpoint.AsStdFunction()};
};

TEST_F(HyperVHCNAPI_UnitTests, create_network)
{
    ::testing::MockFunction<decltype(HcnCreateNetwork)> mock_create_network;

    mock_api_table.CreateNetwork = mock_create_network.AsStdFunction();

    ON_CALL(mock_create_network, Call(_, _, _, _))
        .WillByDefault(DoAll(Invoke([&]() {
                                 // do some stuff
                             }),
                             Return(42)));


    uut_t uut{mock_api_table};
    hyperv::hcn::CreateNetworkParameters params{};
    params.name = "multipass-hyperv-api-hcn-create-delete-test";
    params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
    params.subnet = "172.50.224.0/20";
    params.gateway = "172.50.224.1";

    uut.delete_network(params.guid);

    {
        const auto& [success, error_msg] = uut.create_network(params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = uut.delete_network(params.guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

} // namespace multipass::test