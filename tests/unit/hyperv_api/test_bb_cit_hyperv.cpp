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
#include "tests/unit/common.h"

#include <fmt/xchar.h>

#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_endpoint_info.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_endpoint_naming.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <computecore.h>
#include <multipass/ip_address.h>

#include <scope_guard.hpp>

namespace multipass::test
{

using namespace hyperv::hcs;
using hyperv::hcn::HCN;
using hyperv::virtdisk::VirtDisk;

// Component level big bang integration tests for Hyper-V HCN/HCS + virtdisk API's.
// These tests ensure that the API's working together as expected.
struct HyperV_ComponentIntegrationTests : public ::testing::Test
{
};

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm_on_ics_dhcp_network)
{
    hyperv::hcs::HcsSystemHandle handle{nullptr};
    // 10.0. 0.0 to 10.255. 255.255.
    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit";
        network_parameters.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47486";
        network_parameters.flags = hyperv::hcn::HcnNetworkFlags::enable_dhcp_server;
        network_parameters.ipams = {
            hyperv::hcn::HcnIpam{hyperv::hcn::HcnIpamType::Static(),
                                 {hyperv::hcn::HcnSubnet{"10.99.99.0/24"}}}};
        return network_parameters;
    }();

    const auto endpoint_parameters = [&network_parameters]() {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97029f";
        return endpoint_parameters;
    }();

    auto cleanup = sg::make_scope_guard([&]() noexcept {
        if (handle)
        {
            (void)HCS().terminate_compute_system(handle);
            handle.reset();
        }
        (void)HCN().delete_endpoint(endpoint_parameters.endpoint_guid);
        (void)HCN().delete_network(network_parameters.guid);
    });

    const auto temp_path = make_tempfile_path(".vhdx");

    const hyperv::virtdisk::CreateVirtualDiskParameters create_disk_parameters{
        .size_in_bytes = (1024 * 1024) * 512, // 512 MiB
        .path = temp_path,
        .predecessor = {}};

    const auto network_adapter = [&endpoint_parameters]() {
        hyperv::hcs::HcsNetworkAdapter network_adapter{};
        network_adapter.endpoint_guid = endpoint_parameters.endpoint_guid;
        network_adapter.mac_address = "00-15-5D-9D-CF-69";
        return network_adapter;
    }();

    const auto create_vm_parameters = [&network_adapter]() {
        hyperv::hcs::CreateComputeSystemParameters vm_parameters{};
        vm_parameters.name = "multipass-hyperv-cit-vm";
        vm_parameters.processor_count = 1;
        vm_parameters.memory_size_mb = 512;
        vm_parameters.network_adapters.push_back(network_adapter);
        return vm_parameters;
    }();

    if (HCS().open_compute_system(create_vm_parameters.name, handle))
    {
        (void)HCS().terminate_compute_system(handle);
        handle.reset();
    }

    // Create the test network
    {
        const auto& [status, status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(status.success());
    }

    // Create the test endpoint
    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status.success());
    }

    // Create the test VHDX (empty)
    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(create_disk_parameters);
        ASSERT_TRUE(status.success());
    }

    // Create test VM
    {
        const auto& [status, status_msg] = HCS().create_compute_system(create_vm_parameters,
                                                                       handle);
        ASSERT_TRUE(status.success());
    }

    // Start test VM
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status.success());
    }

    hyperv::hcn::HcnEndpointInfo endpoint_info;
    const auto query_result = HCN().query_endpoint(endpoint_parameters.endpoint_guid,
                                                   endpoint_info);
    ASSERT_TRUE(query_result);
    EXPECT_TRUE(endpoint_info.ip_addresses.empty());

    (void)HCS().terminate_compute_system(handle);
    handle.reset();
    (void)HCN().delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)HCN().delete_network(network_parameters.guid);
    cleanup.dismiss();
}

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm_attach_nic_after_boot)
{
    hyperv::hcs::HcsSystemHandle handle{nullptr};
    // 10.0. 0.0 to 10.255. 255.255.
    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit";
        network_parameters.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47486";
        network_parameters.ipams = {
            hyperv::hcn::HcnIpam{hyperv::hcn::HcnIpamType::Static(),
                                 {hyperv::hcn::HcnSubnet{"10.99.99.0/24"}}}};
        return network_parameters;
    }();

    const auto endpoint_parameters = [&network_parameters]() {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97029f";
        return endpoint_parameters;
    }();

    // Remove remnants from previous tests, if any.
    (void)HCN().delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)HCN().delete_network(network_parameters.guid);

    const auto temp_path = make_tempfile_path(".vhdx");

    const hyperv::virtdisk::CreateVirtualDiskParameters create_disk_parameters{
        .size_in_bytes = (1024 * 1024) * 512, // 512 MiB
        .path = temp_path,
        .predecessor = {}};

    const auto create_vm_parameters = []() {
        hyperv::hcs::CreateComputeSystemParameters vm_parameters{};
        vm_parameters.name = "multipass-hyperv-cit-vm";
        vm_parameters.processor_count = 1;
        vm_parameters.memory_size_mb = 512;
        return vm_parameters;
    }();

    const auto network_adapter = [&endpoint_parameters]() {
        hyperv::hcs::HcsNetworkAdapter network_adapter{};
        network_adapter.endpoint_guid = endpoint_parameters.endpoint_guid;
        network_adapter.mac_address = "00-15-5D-9D-CF-69";
        return network_adapter;
    }();

    // Remove remnants from previous tests, if any.
    {
        if (HCN().delete_endpoint(endpoint_parameters.endpoint_guid))
        {
            GTEST_LOG_(WARNING) << "The test endpoint was already present, deleted it.";
        }
        if (HCN().delete_network(network_parameters.guid))
        {
            GTEST_LOG_(WARNING) << "The test network was already present, deleted it.";
        }

        if (HCS().open_compute_system(create_vm_parameters.name, handle))
        {
            if (HCS().terminate_compute_system(handle))
            {
                GTEST_LOG_(WARNING) << "The test system was already present, terminated it.";
            }
            handle.reset();
        }
    }

    // Create the test network
    {
        const auto& [status, status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    // Create the test endpoint
    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    // Create the test VHDX (empty)
    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(create_disk_parameters);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    // Create test VM
    {
        const auto& [status, status_msg] = HCS().create_compute_system(create_vm_parameters,
                                                                       handle);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    std::string vm_guid{};
    // Start test VM
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
        ASSERT_TRUE(HCS().get_compute_system_guid(handle, vm_guid));
        ASSERT_FALSE(vm_guid.empty());
    }

    // Add network adapter
    {
        const HcsRequest add_network_adapter_req{
            HcsResourcePath::NetworkAdapters(network_adapter.endpoint_guid),
            HcsRequestType::Add(),
            network_adapter};
        const auto& [status, status_msg] = HCS().modify_compute_system(handle,
                                                                       add_network_adapter_req);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    // Verify that endpoint is attached to the VM
    {
        // Create another EP so we can ensure that we're only listing the EPs belonging to the VM
        {
            const auto& [status,
                         status_msg] = HCN().create_endpoint(hyperv::hcn::CreateEndpointParameters{
                .network_guid = network_parameters.guid,
                .endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97029b",
            });

            ASSERT_TRUE(status.success());
            ASSERT_TRUE(status_msg.empty());
        }

        std::vector<std::string> eps{};
        const auto& [status, status_msg] = HCN().enumerate_attached_endpoints(vm_guid, eps);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
        ASSERT_EQ(eps.size(), 1);
        ASSERT_EQ(eps[0], network_adapter.endpoint_guid);
    }

    EXPECT_TRUE(HCS().terminate_compute_system(handle)) << "Terminate system failed!";
    EXPECT_TRUE(HCN().delete_endpoint(endpoint_parameters.endpoint_guid))
        << "Delete endpoint failed!";
    EXPECT_TRUE(HCN().delete_network(network_parameters.guid)) << "Delete network failed!";
    handle.reset();
}

TEST_F(HyperV_ComponentIntegrationTests, endpoints_tagged_with_same_name_are_found_and_removed)
{
    constexpr auto vm_name = "multipass-hyperv-cit-endpoint-leak-vm";
    const auto endpoint_name = hyperv::hcn::endpoint_name_for(vm_name);

    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit-endpoint-leak";
        network_parameters.guid = "c6e6a6c1-9f7e-4b8a-8f0e-6a0a6b6c6d6e";
        network_parameters.ipams = {
            hyperv::hcn::HcnIpam{hyperv::hcn::HcnIpamType::Static(),
                                 {hyperv::hcn::HcnSubnet{"10.99.100.0/24"}}}};
        return network_parameters;
    }();

    const std::vector<std::string> endpoint_guids{"aee79cf9-54d1-4653-81fb-8110db970200",
                                                  "bfe89da0-65e2-5764-92fc-9221ec081311"};

    const auto make_endpoint_parameters = [&network_parameters,
                                           &endpoint_name](const std::string& endpoint_guid) {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = endpoint_guid;
        // Tag the endpoint with the deterministic, instance-based name -- this is what allows
        // it to be found and removed without needing the VM's RuntimeId.
        endpoint_parameters.name = endpoint_name;
        return endpoint_parameters;
    };

    // Remove remnants from previous (failed) test runs, if any.
    {
        std::vector<std::string> stale_endpoints{};
        (void)HCN().find_endpoints_by_name(endpoint_name, stale_endpoints);
        for (const auto& stale : stale_endpoints)
            (void)HCN().delete_endpoint(stale);

        for (const auto& guid : endpoint_guids)
            (void)HCN().delete_endpoint(guid);
        (void)HCN().delete_network(network_parameters.guid);
    }

    // Create the test network
    {
        const auto& [status, status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(status.success());
    }

    // Create both endpoints, tagged with the same instance-based name.
    for (const auto& guid : endpoint_guids)
    {
        const auto& [status, status_msg] = HCN().create_endpoint(make_endpoint_parameters(guid));
        ASSERT_TRUE(status.success());
    }

    // Querying by name discovers *all* endpoints sharing that name.
    {
        std::vector<std::string> found{};
        const auto& [status, status_msg] = HCN().find_endpoints_by_name(endpoint_name, found);
        ASSERT_TRUE(status.success());
        EXPECT_THAT(found, testing::UnorderedElementsAreArray(endpoint_guids));
    }

    // Deleting all discovered endpoints removes them.
    {
        std::vector<std::string> found{};
        const auto& [status, status_msg] = HCN().find_endpoints_by_name(endpoint_name, found);
        ASSERT_TRUE(status.success());

        for (const auto& guid : found)
        {
            const auto& [del_status, del_msg] = HCN().delete_endpoint(guid);
            EXPECT_TRUE(del_status.success());
        }
    }

    // Verify none of them remain -- no leak.
    {
        std::vector<std::string> found{};
        const auto& [status, status_msg] = HCN().find_endpoints_by_name(endpoint_name, found);
        ASSERT_TRUE(status.success());
        EXPECT_TRUE(found.empty());
    }

    (void)HCN().delete_network(network_parameters.guid);
}

} // namespace multipass::test
