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

#include <fmt/xchar.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>

namespace multipass::test
{

using hyperv::hcn::HCN;

struct HyperVHCNAPI_IntegrationTests : public ::testing::Test
{
};

TEST_F(HyperVHCNAPI_IntegrationTests, create_delete_network)
{
    hyperv::hcn::CreateNetworkParameters params{};
    params.name = "multipass-hyperv-api-hcn-create-delete-test";
    params.guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
    params.ipams = {hyperv::hcn::HcnIpam{hyperv::hcn::HcnIpamType::Static(),
                                         {hyperv::hcn::HcnSubnet{"172.50.224.0/20"}}}};

    (void)HCN().delete_network(params.guid);

    {
        const auto& [success, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().delete_network(params.guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, create_delete_endpoint)
{
    hyperv::hcn::CreateNetworkParameters network_params{};
    network_params.name = "multipass-hyperv-api-hcn-create-delete-test";
    network_params.guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
    network_params.ipams = {hyperv::hcn::HcnIpam{hyperv::hcn::HcnIpamType::Static(),
                                                 {hyperv::hcn::HcnSubnet{"172.50.224.0/20"}}}};

    hyperv::hcn::CreateEndpointParameters endpoint_params{};

    endpoint_params.network_guid = network_params.guid;
    endpoint_params.endpoint_guid = "b70c479d-f808-4053-aafa-705bc15b6d70";

    (void)HCN().delete_network(network_params.guid);

    {
        const auto& [success, error_msg] = HCN().create_network(network_params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().create_endpoint(endpoint_params);
        std::wprintf(L"%s\n", error_msg.c_str());
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().delete_endpoint(endpoint_params.endpoint_guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().delete_network(network_params.guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, create_endpoint_explicit_mac)
{
    hyperv::hcn::CreateNetworkParameters network_params{};
    network_params.name = "multipass-hyperv-api-hcn-create-delete-test";
    network_params.guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
    network_params.ipams = {hyperv::hcn::HcnIpam{hyperv::hcn::HcnIpamType::Static(),
                                                 {hyperv::hcn::HcnSubnet{"172.50.224.0/20"}}}};

    hyperv::hcn::CreateEndpointParameters endpoint_params{};

    endpoint_params.network_guid = network_params.guid;
    endpoint_params.endpoint_guid = "b70c479d-f808-4053-aafa-705bc15b6d70";
    endpoint_params.mac_address = "00-11-22-33-44-55";

    (void)HCN().delete_network(network_params.guid);

    {
        const auto& [success, error_msg] = HCN().create_network(network_params);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().create_endpoint(endpoint_params);
        std::wprintf(L"%s\n", error_msg.c_str());
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().delete_endpoint(endpoint_params.endpoint_guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [success, error_msg] = HCN().delete_network(network_params.guid);
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_msg.empty());
    }
}

} // namespace multipass::test
