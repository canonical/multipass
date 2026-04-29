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

#include "tests/unit/common.h"

#include <fmt/xchar.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>

namespace multipass::test
{

using namespace hyperv::hcn;
using hyperv::hcn::HCN;

struct HyperVHCNAPI_IntegrationTests : public ::testing::Test
{
};

TEST_F(HyperVHCNAPI_IntegrationTests, create_delete_network)
{
    CreateNetworkParameters params{};
    params.name = "multipass-hyperv-api-hcn-create-delete-test";
    params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
    params.ipams = {HcnIpam{HcnIpamType::Static(), {HcnSubnet{"172.50.224.0/20"}}}};

    (void)HCN().delete_network(params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().delete_network(params.guid);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, enumerate_networks)
{
    CreateNetworkParameters params{};
    params.name = "multipass-hyperv-api-hcn-enumerate-test";
    params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
    params.ipams = {HcnIpam{HcnIpamType::Static(), {HcnSubnet{"172.50.224.0/20"}}}};

    (void)HCN().delete_network(params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(status.success());
    }

    {
        std::vector<std::string> guids;
        const auto result = HCN().enumerate_networks(guids);
        ASSERT_TRUE(result);
        EXPECT_NE(std::find(guids.cbegin(), guids.cend(), "b70c479d-f808-4053-aafa-705bc15b6d68"),
                  guids.cend());
    }

    {
        const auto& [status, error_msg] = HCN().delete_network(params.guid);
        ASSERT_TRUE(status.success());
    }

    {
        std::vector<std::string> guids;
        const auto result = HCN().enumerate_networks(guids);
        ASSERT_TRUE(result);
        EXPECT_EQ(std::find(guids.cbegin(), guids.cend(), "b70c479d-f808-4053-aafa-705bc15b6d68"),
                  guids.cend());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, query_network)
{
    CreateNetworkParameters params{};
    params.name = "multipass-hyperv-api-hcn-query-test";
    params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
    params.ipams = {HcnIpam{HcnIpamType::Static(), {HcnSubnet{"172.50.224.0/20"}}}};

    (void)HCN().delete_network(params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(status.success());
    }

    {
        HcnNetworkInfo info{};
        const auto result = HCN().query_network("b70c479d-f808-4053-aafa-705bc15b6d68", info);
        ASSERT_TRUE(result);
        EXPECT_EQ(info.name, params.name);
        EXPECT_EQ(info.type, "ICS");
        EXPECT_EQ(info.guid, "b70c479d-f808-4053-aafa-705bc15b6d68");
    }

    {
        const auto& [status, error_msg] = HCN().delete_network(params.guid);
        ASSERT_TRUE(status.success());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, query_nonexistent_network)
{
    HcnNetworkInfo info{};
    const auto result = HCN().query_network("00000000-0000-0000-0000-000000000000", info);
    EXPECT_FALSE(result);
}

TEST_F(HyperVHCNAPI_IntegrationTests, create_delete_endpoint)
{
    CreateNetworkParameters network_params{};
    network_params.name = "multipass-hyperv-api-hcn-create-delete-test";
    network_params.guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
    network_params.ipams = {HcnIpam{HcnIpamType::Static(), {HcnSubnet{"172.50.224.0/20"}}}};

    CreateEndpointParameters endpoint_params{};

    endpoint_params.network_guid = network_params.guid;
    endpoint_params.endpoint_guid = "b70c479d-f808-4053-aafa-705bc15b6d70";

    (void)HCN().delete_network(network_params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(network_params);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().create_endpoint(endpoint_params);
        std::wprintf(L"%s\n", error_msg.c_str());
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().delete_endpoint(endpoint_params.endpoint_guid);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().delete_network(network_params.guid);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, create_endpoint_explicit_mac)
{
    CreateNetworkParameters network_params{};
    network_params.name = "multipass-hyperv-api-hcn-create-delete-test";
    network_params.guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
    network_params.ipams = {HcnIpam{HcnIpamType::Static(), {HcnSubnet{"172.50.224.0/20"}}}};

    CreateEndpointParameters endpoint_params{};

    endpoint_params.network_guid = network_params.guid;
    endpoint_params.endpoint_guid = "b70c479d-f808-4053-aafa-705bc15b6d70";
    endpoint_params.mac_address = "00-11-22-33-44-55";

    (void)HCN().delete_network(network_params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(network_params);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().create_endpoint(endpoint_params);
        std::wprintf(L"%s\n", error_msg.c_str());
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().delete_endpoint(endpoint_params.endpoint_guid);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().delete_network(network_params.guid);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }
}

} // namespace multipass::test
