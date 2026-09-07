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
#include "multipass/test_data_path.h"
#include "tests/unit/common.h"
#include "tests/unit/stub_availability_zone.h"
#include "tests/unit/stub_ssh_key_provider.h"
#include "tests/unit/stub_status_monitor.h"

#include <shared/windows/network_utils.h>

#include <multipass/subnet.h>

#include <fmt/xchar.h>

#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_endpoint_info.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_endpoint_naming.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_wrapper.h>
#include <src/platform/backends/hyperv_api/hcs_virtual_machine.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <computecore.h>
#include <multipass/ip_address.h>

#include <scope_guard.hpp>

#include <chrono>
#include <fstream>
#include <thread>
#include <utility>

namespace multipass::test
{

using namespace hyperv::hcs;
using hyperv::hcn::HCN;
using hyperv::virtdisk::VirtDisk;
using namespace std::chrono_literals;

// Component level big bang integration tests for Hyper-V HCN/HCS + virtdisk API's.
// These tests ensure that the API's working together as expected.
struct HyperV_ComponentIntegrationTests : public ::testing::Test
{
    hyperv::hcs::HcsSystemHandle handle{nullptr};

    static hyperv::hcn::CreateNetworkParameters make_network_parameters(
        hyperv::hcn::HcnNetworkFlags flags = hyperv::hcn::HcnNetworkFlags::none)
    {
        return {.name = "multipass-hyperv-cit",
                .flags = flags,
                .guid = "b4d77a0e-2507-45f0-99aa-c638f3e47486",
                .ipams = {{.type = hyperv::hcn::HcnIpamType::Static(),
                           .subnets = {hyperv::hcn::HcnSubnet{"10.99.99.0/24"}}}}};
    }

    static hyperv::hcn::CreateEndpointParameters make_endpoint_parameters(
        const hyperv::hcn::CreateNetworkParameters& network_parameters,
        std::optional<std::string> mac_address = std::nullopt)
    {
        return {.network_guid = network_parameters.guid,
                .endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97029f",
                .mac_address = std::move(mac_address)};
    }

    static hyperv::hcs::HcsNetworkAdapter make_network_adapter(
        const hyperv::hcn::CreateEndpointParameters& endpoint_parameters,
        std::string mac_address = "00-15-5D-9D-CF-69")
    {
        return {.endpoint_guid = endpoint_parameters.endpoint_guid,
                .mac_address = std::move(mac_address)};
    }

    static hyperv::hcs::CreateComputeSystemParameters make_vm_parameters(
        std::vector<hyperv::hcs::HcsScsiDevice> scsi_devices = {},
        std::vector<hyperv::hcs::HcsNetworkAdapter> network_adapters = {})
    {
        return {.name = "multipass-hyperv-cit-vm",
                .memory_size_mb = 512,
                .processor_count = 1,
                .scsi_devices = std::move(scsi_devices),
                .network_adapters = std::move(network_adapters)};
    }

    void prepare_resources(const std::string& vm_name,
                           const hyperv::hcn::CreateEndpointParameters& endpoint_parameters,
                           const hyperv::hcn::CreateNetworkParameters& network_parameters,
                           std::vector<std::string> additional_endpoint_guids = {})
    {
        this->vm_name = vm_name;
        endpoint_guids = {endpoint_parameters.endpoint_guid};
        endpoint_guids.insert(endpoint_guids.end(),
                              additional_endpoint_guids.begin(),
                              additional_endpoint_guids.end());
        network_guid = network_parameters.guid;
        remove_resources();
    }

    void remove_resources() noexcept
    {
        if (!handle && !vm_name.empty())
            (void)HCS().open_compute_system(vm_name, handle);

        if (handle)
        {
            (void)HCS().terminate_compute_system(handle);
            handle.reset();
        }

        for (const auto& endpoint_guid : endpoint_guids)
            (void)HCN().delete_endpoint(endpoint_guid);

        if (!network_guid.empty())
            (void)HCN().delete_network(network_guid);
    }

    void cleanup_resources() noexcept
    {
        remove_resources();
        vm_name.clear();
        endpoint_guids.clear();
        network_guid.clear();
    }

    auto cleanup_guard()
    {
        return sg::make_scope_guard([this]() noexcept { cleanup_resources(); });
    }

    void create_network_and_endpoint(
        const hyperv::hcn::CreateNetworkParameters& network_parameters,
        const hyperv::hcn::CreateEndpointParameters& endpoint_parameters)
    {
        const auto& [network_status, network_status_msg] =
            HCN().create_network(network_parameters);
        ASSERT_TRUE(network_status.success());
        ASSERT_TRUE(network_status_msg.empty());

        const auto& [endpoint_status, endpoint_status_msg] =
            HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(endpoint_status.success());
        ASSERT_TRUE(endpoint_status_msg.empty());
    }

    void TearDown() override
    {
        cleanup_resources();
    }

private:
    std::string vm_name;
    std::vector<std::string> endpoint_guids;
    std::string network_guid;
};

TEST_F(HyperV_ComponentIntegrationTests, alpine_vm_gets_permanent_neighbor_on_ics_dhcp_network)
{
    // 10.0. 0.0 to 10.255. 255.255.
    const auto network_parameters =
        make_network_parameters(hyperv::hcn::HcnNetworkFlags::enable_dhcp_server);
    const auto endpoint_parameters =
        make_endpoint_parameters(network_parameters, "52-54-00-E9-36-7E");
    prepare_resources("multipass-hyperv-cit-vm", endpoint_parameters, network_parameters);

    const auto temp_path = make_tempfile_path(".vhdx");
    const auto cloud_init_iso_path = std::filesystem::path{test_data_path} / "cloud-init" /
                                     "cloud-init.iso";
    auto cleanup = cleanup_guard();
    {
        std::ofstream output{static_cast<const std::filesystem::path&>(temp_path),
                             std::ios::binary};
        ASSERT_TRUE(output);
        for (const auto suffix : {"aa", "ab", "ac"})
        {
            const auto part = std::filesystem::path{test_data_path} / "cloud-vhdx" /
                              fmt::format("alpine.vhdx.part-{}", suffix);
            std::ifstream input{part, std::ios::binary};
            ASSERT_TRUE(input);
            output << input.rdbuf();
        }
    }

    const auto network_adapter =
        make_network_adapter(endpoint_parameters, *endpoint_parameters.mac_address);
    const auto create_vm_parameters = make_vm_parameters(
        {{.type = hyperv::hcs::HcsScsiDeviceType::VirtualDisk(),
          .name = "Primary disk",
          .path = temp_path},
         {.type = hyperv::hcs::HcsScsiDeviceType::Iso(),
          .name = "Cloud-init ISO",
          .path = cloud_init_iso_path,
          .read_only = true}},
        {network_adapter});

    ASSERT_NO_FATAL_FAILURE(create_network_and_endpoint(network_parameters, endpoint_parameters));

    // Create test VM
    {
        const auto& [status, status_msg] = HCS().create_compute_system(create_vm_parameters,
                                                                       handle);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(HCS().grant_vm_access(create_vm_parameters.name, temp_path));
        ASSERT_TRUE(HCS().grant_vm_access(create_vm_parameters.name, cloud_init_iso_path));
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
    ASSERT_TRUE(endpoint_info.mac_address);

    // Windows exposes no notification for IP neighbor-table changes. Poll until
    // the ICS DHCP service creates the permanent neighbor entry for the guest.
    std::optional<std::string> neighbor_address;
    for (auto attempts = 0; attempts < 120 && !neighbor_address; ++attempts)
    {
        neighbor_address = windows_network_utils().permanent_ipv4_neighbor(
            *endpoint_info.mac_address);
        if (!neighbor_address)
            std::this_thread::sleep_for(500ms);
    }
    ASSERT_TRUE(neighbor_address);

}

TEST_F(HyperV_ComponentIntegrationTests, hcs_vm_gets_host_assigned_ipv4_from_hcn)
{
    const hyperv::hcn::CreateNetworkParameters network_parameters{
        .name = "multipass-hyperv-hcn-ip-cit",
        .guid = "b4d77a0e-2507-45f0-99aa-c638f3e47487",
        .ipams = {{.type = hyperv::hcn::HcnIpamType::Static(),
                   .subnets = {hyperv::hcn::HcnSubnet{"10.99.100.0/24"}}}}};

    const std::string vm_name{"multipass-hyperv-hcn-ip-cit-vm"};
    const std::string mac_address{"00:15:5d:9d:cf:69"};
    const hyperv::hcn::CreateEndpointParameters endpoint_parameters{
        .network_guid = network_parameters.guid,
        .endpoint_guid = "db4bdbf0-dc14-407f-9780-00155d9dcf69",
        .mac_address = "00-15-5D-9D-CF-69"};

    prepare_resources(vm_name, endpoint_parameters, network_parameters);
    auto cleanup = cleanup_guard();
    ASSERT_NO_FATAL_FAILURE(create_network_and_endpoint(network_parameters, endpoint_parameters));

    {
        const hyperv::hcs::CreateComputeSystemParameters parameters{
            .name = vm_name,
            .memory_size_mb = 512,
            .processor_count = 1};

        const auto& [status, status_msg] = HCS().create_compute_system(parameters, handle);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    StubAvailabilityZone zone;
    StubSSHKeyProvider key_provider;
    StubVMStatusMonitor monitor;
    const VirtualMachineDescription description{1,
                                                MemorySize{"512M"},
                                                MemorySize{},
                                                vm_name,
                                                zone.get_name(),
                                                mac_address,
                                                {},
                                                "",
                                                {"", "", "", "", {}, {}},
                                                "",
                                                {},
                                                {},
                                                {},
                                                {}};

    {
        hyperv::HCSVirtualMachine vm{network_parameters.guid,
                                     description,
                                     monitor,
                                     key_provider,
                                     zone,
                                     {}};
        const auto address = vm.management_ipv4();

        ASSERT_TRUE(address);
        EXPECT_TRUE(Subnet{"10.99.100.0/24"}.contains(*address));
    }
}

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm)
{
    // 10.0. 0.0 to 10.255. 255.255.
    const auto network_parameters = make_network_parameters();
    const auto endpoint_parameters = make_endpoint_parameters(network_parameters);
    const auto create_vm_parameters =
        make_vm_parameters({}, {make_network_adapter(endpoint_parameters)});
    prepare_resources(create_vm_parameters.name, endpoint_parameters, network_parameters);

    const auto temp_path = make_tempfile_path(".vhdx");
    auto cleanup = cleanup_guard();

    const hyperv::virtdisk::CreateVirtualDiskParameters create_disk_parameters{
        .size_in_bytes = (1024 * 1024) * 512, // 512 MiB
        .path = temp_path,
        .predecessor = {}};

    ASSERT_NO_FATAL_FAILURE(create_network_and_endpoint(network_parameters, endpoint_parameters));

    // Create the test VHDX (empty)
    {
        const auto& [status, status_msg] = VirtDisk().create_virtual_disk(create_disk_parameters);
        ASSERT_TRUE(status.success());
    }

    // Create test VM
    {
        const auto& [status, status_msg] =
            HCS().create_compute_system(create_vm_parameters, handle);
        ASSERT_TRUE(status.success());
    }

    // Start test VM
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status.success());
    }

}

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm_attach_nic_after_boot)
{
    // 10.0. 0.0 to 10.255. 255.255.
    const auto network_parameters = make_network_parameters();
    const auto endpoint_parameters = make_endpoint_parameters(network_parameters);
    const auto create_vm_parameters = make_vm_parameters();
    const auto network_adapter = make_network_adapter(endpoint_parameters);
    constexpr auto extra_endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97029b";
    prepare_resources(create_vm_parameters.name,
                      endpoint_parameters,
                      network_parameters,
                      {extra_endpoint_guid});

    const auto temp_path = make_tempfile_path(".vhdx");
    auto cleanup = cleanup_guard();

    const hyperv::virtdisk::CreateVirtualDiskParameters create_disk_parameters{
        .size_in_bytes = (1024 * 1024) * 512, // 512 MiB
        .path = temp_path,
        .predecessor = {}};

    ASSERT_NO_FATAL_FAILURE(create_network_and_endpoint(network_parameters, endpoint_parameters));

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
                .endpoint_guid = extra_endpoint_guid,
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
