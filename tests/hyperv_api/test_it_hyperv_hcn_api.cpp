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
#include "tests/mock_logger.h"

#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_api_wrapper.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>


namespace mpt = multipass::test;

namespace multipass::test
{

using uut_t = hyperv::hcn::HCNWrapper;

struct HyperVHCNAPI : public ::testing::Test
{

    // mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    void SetUp() override
    {
        //   logger_scope.mock_logger.
    }

    void TearDown() override
    {
    }
};

TEST_F(HyperVHCNAPI, create_delete_network)
{
    uut_t uut;
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

TEST_F(HyperVHCNAPI, create_delete_endpoint)
{
    uut_t uut;
    hyperv::hcn::CreateNetworkParameters network_params{};
    network_params.name = "multipass-hyperv-api-hcn-create-delete-test";
    network_params.guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
    network_params.subnet = "172.50.224.0/20";
    network_params.gateway = "172.50.224.1";

    hyperv::hcn::CreateEndpointParameters endpoint_params{};

    endpoint_params.network_guid = network_params.guid;
    endpoint_params.endpoint_guid = "b70c479d-f808-4053-aafa-705bc15b6d70";
    endpoint_params.vm_creator_id = R"(456fe1ac-1e46-49ec-bb9f-5e44cc2de23b)";
    endpoint_params.endpoint_ipvx_addr = "172.50.224.2";

    uut.delete_network(network_params.guid);

    {
        const auto& [success, error_msg] = uut.create_network(network_params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = uut.create_endpoint(endpoint_params);
        std::wprintf(L"%s\n", error_msg.c_str());
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = uut.delete_endpoint(endpoint_params.endpoint_guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = uut.delete_network(network_params.guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

} // namespace multipass::test
