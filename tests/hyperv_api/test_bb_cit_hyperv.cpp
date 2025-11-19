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

#include <fmt/xchar.h>

#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_api_wrapper.h>

namespace multipass::test
{

using virtdisk_wrapper_t = multipass::hyperv::virtdisk::VirtDiskWrapper;

using namespace hyperv::hcs;
using hyperv::hcn::HCN;

// Component level big bang integration tests for Hyper-V HCN/HCS + virtdisk API's.
// These tests ensure that the API's working together as expected.
struct HyperV_ComponentIntegrationTests : public ::testing::Test
{
};

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm)
{
    virtdisk_wrapper_t virtdisk{};
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

    const auto temp_path = make_tempfile_path(".vhdx");

    const auto create_disk_parameters = [&temp_path]() {
        hyperv::virtdisk::CreateVirtualDiskParameters create_disk_parameters{};
        create_disk_parameters.path = temp_path;
        create_disk_parameters.size_in_bytes = (1024 * 1024) * 512; // 512 MiB
        return create_disk_parameters;
    }();

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
        ASSERT_TRUE(status);
    }

    // Create the test endpoint
    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status);
    }

    // Create the test VHDX (empty)
    {
        const auto& [status, status_msg] = virtdisk.create_virtual_disk(create_disk_parameters);
        ASSERT_TRUE(status);
    }

    // Create test VM
    {
        const auto& [status, status_msg] =
            HCS().create_compute_system(create_vm_parameters, handle);
        ASSERT_TRUE(status);
    }

    // Start test VM
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status);
    }

    (void)HCS().terminate_compute_system(handle);
    handle.reset();
    (void)HCN().delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)HCN().delete_network(network_parameters.guid);
}

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm_attach_nic_after_boot)
{
    virtdisk_wrapper_t virtdisk{};
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

    const auto create_disk_parameters = [&temp_path]() {
        hyperv::virtdisk::CreateVirtualDiskParameters create_disk_parameters{};
        create_disk_parameters.path = temp_path;
        create_disk_parameters.size_in_bytes = (1024 * 1024) * 512; // 512 MiB
        return create_disk_parameters;
    }();

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
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }

    // Create the test endpoint
    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }

    // Create the test VHDX (empty)
    {
        const auto& [status, status_msg] = virtdisk.create_virtual_disk(create_disk_parameters);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }

    // Create test VM
    {
        const auto& [status, status_msg] =
            HCS().create_compute_system(create_vm_parameters, handle);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }

    // Start test VM
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }

    // Add network adapter
    {
        const HcsRequest add_network_adapter_req{
            HcsResourcePath::NetworkAdapters(network_adapter.endpoint_guid),
            HcsRequestType::Add(),
            network_adapter};
        const auto& [status, status_msg] =
            HCS().modify_compute_system(handle, add_network_adapter_req);
        ASSERT_TRUE(status);
        ASSERT_TRUE(status_msg.empty());
    }

    EXPECT_TRUE(HCS().terminate_compute_system(handle)) << "Terminate system failed!";
    EXPECT_TRUE(HCN().delete_endpoint(endpoint_parameters.endpoint_guid))
        << "Delete endpoint failed!";
    EXPECT_TRUE(HCN().delete_network(network_parameters.guid)) << "Delete network failed!";
    handle.reset();
}

} // namespace multipass::test
