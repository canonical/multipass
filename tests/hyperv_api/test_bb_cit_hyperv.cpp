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

#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_api_wrapper.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_api_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_api_wrapper.h>

namespace multipass::test
{

using hcn_wrapper_t = hyperv::hcn::HCNWrapper;
using hcs_wrapper_t = hyperv::hcs::HCSWrapper;
using virtdisk_wrapper_t = multipass::hyperv::virtdisk::VirtDiskWrapper;

// Component level big bang integration tests for Hyper-V HCN/HCS + virtdisk API's.
// These tests ensure that the API's working together as expected.
struct HyperV_ComponentIntegrationTests : public ::testing::Test
{
};

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm)
{
    hcn_wrapper_t hcn{};
    hcs_wrapper_t hcs{};
    virtdisk_wrapper_t virtdisk{};
    // 10.0. 0.0 to 10.255. 255.255.
    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit";
        network_parameters.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47486";
        network_parameters.subnet = "10.99.99.0/24";
        network_parameters.gateway = "10.99.99.1";
        return network_parameters;
    }();

    const auto endpoint_parameters = [&network_parameters]() {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97029f";
        endpoint_parameters.endpoint_ipvx_addr = "10.99.99.10";
        return endpoint_parameters;
    }();

    // Remove remnants from previous tests, if any.
    (void)hcn.delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)hcn.delete_network(network_parameters.guid);

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

    (void)hcs.terminate_compute_system(create_vm_parameters.name);

    const auto add_endpoint_parameters = [&create_vm_parameters, &endpoint_parameters]() {
        hyperv::hcs::AddEndpointParameters add_endpoint_parameters{};
        add_endpoint_parameters.endpoint_guid = endpoint_parameters.endpoint_guid;
        add_endpoint_parameters.target_compute_system_name = create_vm_parameters.name;
        add_endpoint_parameters.nic_mac_address = "00-15-5D-9D-CF-69";
        return add_endpoint_parameters;
    }();

    // Create the test network
    {
        const auto& [status, status_msg] = hcn.create_network(network_parameters);
        ASSERT_TRUE(status);
    }

    // Create the test endpoint
    {
        const auto& [status, status_msg] = hcn.create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status);
    }

    // Create the test VHDX (empty)
    {
        const auto& [status, status_msg] = virtdisk.create_virtual_disk(create_disk_parameters);
        ASSERT_TRUE(status);
    }

    // Create test VM
    {
        const auto& [status, status_msg] = hcs.create_compute_system(create_vm_parameters);
        ASSERT_TRUE(status);
    }

    // Start test VM
    {
        const auto& [status, status_msg] = hcs.start_compute_system(create_vm_parameters.name);
        ASSERT_TRUE(status);
    }

    // Add endpoint
    {
        const auto& [status, status_msg] = hcs.add_endpoint(add_endpoint_parameters);
        fmt::print(L"{}", status_msg.c_str());
        ASSERT_TRUE(status);
    }

    (void)hcs.terminate_compute_system(create_vm_parameters.name);
    (void)hcn.delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)hcn.delete_network(network_parameters.guid);
}

} // namespace multipass::test
