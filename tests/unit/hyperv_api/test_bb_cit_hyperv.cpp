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

#include <fmt/xchar.h>

#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <src/platform/backends/hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_event_type.h>
#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_wrapper.h>
#include <src/platform/backends/hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <future>
#include <thread>

namespace multipass::test
{

using namespace hyperv::hcs;
using hyperv::hcn::HCN;
using hyperv::virtdisk::VirtDisk;

namespace
{
// Reassemble the split alpine.vhdx test image into a fresh temporary VHDX file.
void reassemble_alpine_vhdx(const std::filesystem::path& output)
{
    const auto parts = find_split_parts(fmt::format("{}/cloud-vhdx", test_data_path),
                                        "alpine.vhdx.part-");
    ASSERT_EQ(parts.size(), 3);
    merge_files(parts, output);
}

// Runs a PowerShell command and returns its trimmed stdout.
std::string run_powershell(const std::string& command)
{
    const auto full_command = fmt::format("powershell -NoProfile -NonInteractive -Command \"{}\"",
                                          command);
    std::string output;
    if (FILE* pipe = _popen(full_command.c_str(), "r"))
    {
        char buffer[512];
        while (std::fgets(buffer, sizeof(buffer), pipe))
            output += buffer;
        _pclose(pipe);
    }
    // Trim trailing whitespace/newlines.
    while (!output.empty() && std::isspace(static_cast<unsigned char>(output.back())))
        output.pop_back();
    return output;
}

// Queries the given DNS server for an A record and returns the resolved IPv4 address (if any).
std::string resolve_via(const std::string& dns_server, const std::string& name)
{
    return run_powershell(fmt::format(
        "(Resolve-DnsName -Server {} -Name {} -Type A -DnsOnly -QuickTimeout "
        "-ErrorAction SilentlyContinue | Where-Object {{ $_.Type -eq 'A' }}).IPAddress -join ','",
        dns_server,
        name));
}

// Returns the IPv4 address the host sees for the given MAC address (i.e. the guest's leased
// address, as recorded in the host neighbor/ARP table). Empty if not yet present.
std::string neighbor_ip_for_mac(const std::string& mac_dashed,
                                const std::string& subnet_glob = "172.50.*")
{
    return run_powershell(
        fmt::format("(Get-NetNeighbor -ErrorAction SilentlyContinue | Where-Object {{ "
                    "$_.LinkLayerAddress -eq '{}' -and $_.IPAddress -like '{}' }} | "
                    "Select-Object -First 1).IPAddress",
                    mac_dashed,
                    subnet_glob));
}

} // namespace

// Component level big bang integration tests for Hyper-V HCN/HCS + virtdisk API's.
// These tests ensure that the API's working together as expected.
struct HyperV_ComponentIntegrationTests : public ::testing::Test
{
};

TEST_F(HyperV_ComponentIntegrationTests, spawn_empty_test_vm)
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
        const auto& [status, status_msg] =
            HCS().create_compute_system(create_vm_parameters, handle);
        ASSERT_TRUE(status.success());
    }

    // Start test VM
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status.success());
    }

    (void)HCS().terminate_compute_system(handle);
    handle.reset();
    (void)HCN().delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)HCN().delete_network(network_parameters.guid);
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
        const auto& [status, status_msg] =
            HCS().create_compute_system(create_vm_parameters, handle);
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
        const auto& [status, status_msg] =
            HCS().modify_compute_system(handle, add_network_adapter_req);
        ASSERT_TRUE(status.success());
        ASSERT_TRUE(status_msg.empty());
    }

    // Verify that endpoint is attached to the VM
    {
        // Create another EP so we can ensure that we're only listing the EPs belonging to the VM
        {
            const auto& [status, status_msg] =
                HCN().create_endpoint(hyperv::hcn::CreateEndpointParameters{
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

// End-to-end test: boot a real (minimal Alpine) guest on an ICS network that has a DNS
// suffix configured and verify that the host can resolve the guest by name to its leased IP.
//
// Two notes on what this exercises:
//   * The guest must advertise its hostname over DHCP (option 12) for the ICS DHCP/DNS proxy
//     to register it. Stock Alpine's busybox udhcpc does not, so we boot it with the
//     "cloud-init-dhcp-hostname" seed that re-runs udhcpc with the hostname.
//   * The ICS DNS proxy registers leased hostnames under the machine-wide ICS domain
//     (HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\ICSDomain, default
//     "mshome.net"). That domain -- not the per-network HCN DNS suffix -- determines the FQDN
//     under which the host can resolve the guest. The per-network HCN DNS block is a
//     client-side hint pushed into the guest's resolver. We therefore resolve the guest under
//     the default ICS domain and assert it maps to the endpoint's actual leased address.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_hostname_resolution)
{
    constexpr auto dns_suffix = "mshome.net";
    constexpr auto guest_hostname = "multipass-alpine-it"; // set by cloud-init meta-data
    constexpr auto gateway_ip = "172.50.224.1";
    constexpr auto ics_domain = "mshome.net"; // machine-wide ICS registration domain
    constexpr auto adapter_mac = "00-15-5D-9D-CF-70";
    const auto guest_fqdn = fmt::format("{}.{}", guest_hostname, ics_domain);

    hyperv::hcs::HcsSystemHandle handle{nullptr};

    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit-dns";
        network_parameters.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47487";
        network_parameters.flags = hyperv::hcn::HcnNetworkFlags::enable_dns_proxy |
                                   hyperv::hcn::HcnNetworkFlags::enable_dhcp_server;
        network_parameters.ipams = {hyperv::hcn::HcnIpam{
            hyperv::hcn::HcnIpamType::Static(),
            {hyperv::hcn::HcnSubnet{"172.50.224.0/20",
                                    {hyperv::hcn::HcnRoute{gateway_ip, "0.0.0.0/0", 0}}}}}};
        network_parameters.dns = hyperv::hcn::HcnDns{/* domain */ dns_suffix,
                                                     /* search */ {dns_suffix},
                                                     /* server_list */ {gateway_ip},
                                                     /* options */ {}};
        return network_parameters;
    }();

    const auto endpoint_parameters = [&network_parameters]() {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97030a";
        return endpoint_parameters;
    }();

    const auto network_adapter = [&endpoint_parameters]() {
        hyperv::hcs::HcsNetworkAdapter network_adapter{};
        network_adapter.endpoint_guid = endpoint_parameters.endpoint_guid;
        network_adapter.mac_address = adapter_mac;
        return network_adapter;
    }();

    const auto vhdx_path = make_tempfile_path(".vhdx");
    reassemble_alpine_vhdx(vhdx_path);
    const std::filesystem::path cloud_init_iso_path{
        fmt::format("{}/cloud-init-dhcp-hostname/cloud-init.iso", test_data_path)};
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(vhdx_path)));
    ASSERT_TRUE(std::filesystem::exists(cloud_init_iso_path));

    const auto vm_name = "multipass-hyperv-cit-dns-vm";

    const auto create_vm_parameters = [&]() {
        hyperv::hcs::CreateComputeSystemParameters vm_parameters{};
        vm_parameters.name = vm_name;
        vm_parameters.processor_count = 1;
        vm_parameters.memory_size_mb = 512;
        vm_parameters.network_adapters.push_back(network_adapter);
        vm_parameters.scsi_devices = {
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::VirtualDisk(),
                                       .name = "Primary disk",
                                       .path = vhdx_path},
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::Iso(),
                                       .name = "Cloud-init ISO",
                                       .path = cloud_init_iso_path,
                                       .read_only = true}};
        return vm_parameters;
    }();

    // Remove remnants from previous runs, if any.
    if (HCS().open_compute_system(vm_name, handle))
    {
        (void)HCS().terminate_compute_system(handle);
        handle.reset();
    }
    (void)HCN().delete_endpoint(endpoint_parameters.endpoint_guid);
    (void)HCN().delete_network(network_parameters.guid);

    // Create the DNS-suffixed network.
    {
        const auto& [status, status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(status.success()) << status_msg;
    }

    // Create the endpoint.
    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status.success()) << status_msg;
    }

    // Create and boot the Alpine VM.
    {
        const auto& [status, status_msg] = HCS().create_compute_system(create_vm_parameters,
                                                                       handle);
        ASSERT_TRUE(status.success()) << status_msg;
    }
    ASSERT_TRUE(HCS().grant_vm_access(vm_name, vhdx_path));
    ASSERT_TRUE(HCS().grant_vm_access(vm_name, cloud_init_iso_path));
    {
        const auto& [status, status_msg] = HCS().start_compute_system(handle);
        ASSERT_TRUE(status.success()) << status_msg;
    }

    // Avoid resolving a stale record cached from an earlier run.
    (void)run_powershell("Clear-DnsClientCache");

    // Poll until the guest boots and leases an address (visible to the host via its MAC), and
    // the ICS DNS proxy resolves the guest's name to that very address. Requiring the resolved
    // address to match the current lease guards against stale registrations from earlier runs.
    std::string leased_ip{};
    std::string resolved_ip{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (std::chrono::steady_clock::now() < deadline)
    {
        leased_ip = neighbor_ip_for_mac(adapter_mac);
        if (!leased_ip.empty())
        {
            resolved_ip = resolve_via(gateway_ip, guest_fqdn);
            if (resolved_ip == leased_ip)
                break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    GTEST_LOG_(INFO) << "Resolved " << guest_fqdn << " -> '" << resolved_ip << "' (guest leased '"
                     << leased_ip << "')";

    ASSERT_FALSE(leased_ip.empty())
        << "Guest never obtained a DHCP lease (did it boot and bring up its NIC?)";
    EXPECT_TRUE(leased_ip.starts_with("172.50."))
        << "Guest leased an unexpected address: " << leased_ip;
    EXPECT_EQ(resolved_ip, leased_ip) << guest_fqdn << " did not resolve, via the ICS DNS proxy at "
                                      << gateway_ip << ", to the endpoint's leased address";

    // Clean up.
    {
        std::promise<void> exited;
        auto fut = exited.get_future();
        (void)HCS().set_compute_system_callback(
            handle,
            &exited,
            [](HCS_EVENT* event, void* context) {
                if (event && context &&
                    hyperv::hcs::parse_event(event) == hyperv::hcs::HcsEventType::SystemExited)
                {
                    static_cast<std::promise<void>*>(context)->set_value();
                }
            });
        EXPECT_TRUE(HCS().terminate_compute_system(handle)) << "Terminate system failed!";
        (void)fut.wait_for(std::chrono::seconds(60));
        handle.reset();
    }
    EXPECT_TRUE(HCN().delete_endpoint(endpoint_parameters.endpoint_guid))
        << "Delete endpoint failed!";
    EXPECT_TRUE(HCN().delete_network(network_parameters.guid)) << "Delete network failed!";
}

} // namespace multipass::test
