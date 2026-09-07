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

#include <multipass/subnet.h>

#include <fmt/xchar.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_endpoint_info.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>

#include <scope_guard.hpp>

#include <optional>
#include <string>
#include <utility>

namespace multipass::test
{

using namespace hyperv::hcn;
using hyperv::hcn::HCN;

struct HyperVHCNAPI_IntegrationTests : public ::testing::Test
{
    inline static constexpr auto network_guid = "b70c479d-f808-4053-aafa-705bc15b6d68";
    inline static constexpr auto braced_network_guid = "{b70c479d-f808-4053-aafa-705bc15b6d68}";
    inline static constexpr auto endpoint_guid = "b70c479d-f808-4053-aafa-705bc15b6d70";
    inline static constexpr auto subnet = "172.50.224.0/20";

    static CreateNetworkParameters make_network_parameters(const std::string& name,
                                                           bool use_braced_guid = false)
    {
        return {.name = name,
                .guid = use_braced_guid ? braced_network_guid : network_guid,
                .ipams = {{.type = HcnIpamType::Static(), .subnets = {HcnSubnet{subnet}}}}};
    }

    static CreateEndpointParameters make_endpoint_parameters(
        const CreateNetworkParameters& network_parameters,
        std::optional<std::string> mac_address = std::nullopt)
    {
        return {.network_guid = network_parameters.guid,
                .endpoint_guid = endpoint_guid,
                .mac_address = std::move(mac_address)};
    }
};

TEST_F(HyperVHCNAPI_IntegrationTests, create_delete_network)
{
    const auto params = make_network_parameters("multipass-hyperv-api-hcn-create-delete-test",
                                                true);

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
    const auto params = make_network_parameters("multipass-hyperv-api-hcn-enumerate-test", true);

    (void)HCN().delete_network(params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(status.success());
    }

    {
        std::vector<std::string> guids;
        const auto result = HCN().enumerate_networks(guids);
        ASSERT_TRUE(result);
        EXPECT_NE(std::find(guids.cbegin(), guids.cend(), network_guid), guids.cend());
    }

    {
        const auto& [status, error_msg] = HCN().delete_network(params.guid);
        ASSERT_TRUE(status.success());
    }

    {
        std::vector<std::string> guids;
        const auto result = HCN().enumerate_networks(guids);
        ASSERT_TRUE(result);
        EXPECT_EQ(std::find(guids.cbegin(), guids.cend(), network_guid), guids.cend());
    }
}

TEST_F(HyperVHCNAPI_IntegrationTests, query_network)
{
    const auto params = make_network_parameters("multipass-hyperv-api-hcn-query-test", true);

    (void)HCN().delete_network(params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(params);
        ASSERT_TRUE(status.success());
    }

    {
        HcnNetworkInfo info{};
        const auto result = HCN().query_network(network_guid, info);
        ASSERT_TRUE(result);
        EXPECT_EQ(info.name, params.name);
        EXPECT_EQ(info.type, "ICS");
        EXPECT_EQ(info.guid, network_guid);
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
    const auto network_params = make_network_parameters(
        "multipass-hyperv-api-hcn-create-delete-test");
    const auto endpoint_params = make_endpoint_parameters(network_params);

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

TEST_F(HyperVHCNAPI_IntegrationTests, query_endpoint_returns_host_assigned_ipv4)
{
    const auto network_params = make_network_parameters(
        "multipass-hyperv-api-hcn-query-endpoint-test");
    const auto endpoint_params = make_endpoint_parameters(network_params);

    auto cleanup = sg::make_scope_guard([&]() noexcept {
        (void)HCN().delete_endpoint(endpoint_params.endpoint_guid);
        (void)HCN().delete_network(network_params.guid);
    });

    (void)HCN().delete_endpoint(endpoint_params.endpoint_guid);
    (void)HCN().delete_network(network_params.guid);

    {
        const auto& [status, error_msg] = HCN().create_network(network_params);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    {
        const auto& [status, error_msg] = HCN().create_endpoint(endpoint_params);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(error_msg.empty());
    }

    HcnEndpointInfo endpoint_info;
    const auto result = HCN().query_endpoint(endpoint_params.endpoint_guid, endpoint_info);

    ASSERT_TRUE(result);
    ASSERT_EQ(endpoint_info.ip_addresses.size(), 1);
    EXPECT_TRUE(Subnet{subnet}.contains(IPAddress{endpoint_info.ip_addresses.front()}));
}

TEST_F(HyperVHCNAPI_IntegrationTests, create_endpoint_explicit_mac)
{
    const auto network_params = make_network_parameters(
        "multipass-hyperv-api-hcn-create-delete-test");
    const auto endpoint_params = make_endpoint_parameters(network_params, "00-11-22-33-44-55");

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
