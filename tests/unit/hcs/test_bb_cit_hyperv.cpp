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

#include <multipass/subnet.h>

#include <fmt/xchar.h>

#include <src/platform/backends/hcs/api/dns/nrpt_rule.h>
#include <src/platform/backends/hcs/api/hcn/hcn_create_endpoint_params.h>
#include <src/platform/backends/hcs/api/hcn/hcn_create_network_params.h>
#include <src/platform/backends/hcs/api/hcn/hcn_endpoint_info.h>
#include <src/platform/backends/hcs/api/hcn/hcn_endpoint_naming.h>
#include <src/platform/backends/hcs/api/hcn/hcn_wrapper.h>
#include <src/platform/backends/hcs/api/hcs/hcs_event_type.h>
#include <src/platform/backends/hcs/api/hcs/hcs_wrapper.h>
#include <src/platform/backends/hcs/api/virtdisk/virtdisk_wrapper.h>
#include <src/platform/backends/hcs/hcs_virtual_machine.h>
#include <src/platform/backends/hcs/hcs_virtual_machine_resources.h>

#include <computecore.h>
#include <multipass/ip_address.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <random>
#include <thread>
#include <utility>

namespace multipass::test
{

using namespace hyperv::hcs;
using hyperv::hcn::HCN;
using hyperv::virtdisk::VirtDisk;
using namespace std::chrono_literals;

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

// PowerShell-style registry path of the DNS client parameters (suffix append, NRPT lives below).
constexpr auto dnscache_key = "HKLM:\\SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters";

// A hostname unique to this run. The ICS DNS proxy keeps the registrations of earlier runs (for
// about a week), so a reused name could resolve to a stale lease and mask a broken registration.
std::string unique_hostname(const std::string& prefix)
{
    std::mt19937_64 rng{std::random_device{}()};
    return fmt::format("{}-{:08x}", prefix, static_cast<std::uint32_t>(rng()));
}

// Builds a NoCloud seed ISO at `iso` whose guest advertises `hostname` over DHCP (option 12), so
// that the ICS DNS proxy registers it. Requires Python with pycdlib.
void build_seed_iso(const std::string& hostname, const std::filesystem::path& iso)
{
    const auto output = run_powershell(
        fmt::format("python '{}/cloud-init-fqdn-multipass/make_seed_iso.py' --hostname '{}' "
                    "--output '{}' 2>&1",
                    test_data_path,
                    hostname,
                    iso.string()));
    ASSERT_TRUE(std::filesystem::exists(iso)) << "seed ISO build failed: " << output;
}

// Makes the host's DNS client retry a multi-label name that fails to resolve with `suffix`
// appended (SuffixSearchList + AppendToMultiLabelName). Both settings are machine-wide, so they
// are restored on scope exit.
class ScopedSuffixAppend
{
public:
    explicit ScopedSuffixAppend(const std::string& suffix)
        : saved_list{run_powershell("(Get-DnsClientGlobalSetting).SuffixSearchList -join ','")},
          saved_amln{run_powershell(fmt::format(
              "$v=(Get-ItemProperty -Path '{}' -Name AppendToMultiLabelName -EA SilentlyContinue)."
              "AppendToMultiLabelName; if ($null -eq $v) {{ 'UNSET' }} else {{ $v }}",
              dnscache_key))}
    {
        (void)run_powershell(fmt::format(
            "Set-DnsClientGlobalSetting -SuffixSearchList @('{}'); "
            "Set-ItemProperty -Path '{}' -Name AppendToMultiLabelName -Value 1 -Type DWord",
            suffix,
            dnscache_key));
    }

    ~ScopedSuffixAppend()
    {
        (void)run_powershell(
            saved_list.empty()
                ? std::string{"Set-DnsClientGlobalSetting -SuffixSearchList @()"}
                : fmt::format("Set-DnsClientGlobalSetting -SuffixSearchList ('{}' -split ',')",
                              saved_list));
        (void)run_powershell(
            saved_amln == "UNSET"
                ? fmt::format("Remove-ItemProperty -Path '{}' -Name AppendToMultiLabelName "
                              "-EA SilentlyContinue",
                              dnscache_key)
                : fmt::format("Set-ItemProperty -Path '{}' -Name AppendToMultiLabelName -Value {} "
                              "-Type DWord",
                              dnscache_key,
                              saved_amln));
        (void)run_powershell("Clear-DnsClientCache");
    }

    ScopedSuffixAppend(const ScopedSuffixAppend&) = delete;
    ScopedSuffixAppend& operator=(const ScopedSuffixAppend&) = delete;

private:
    std::string saved_list;
    std::string saved_amln;
};

// Removes the given compute systems, endpoints and networks up front (remnants of an interrupted
// run) and again on scope exit, so that a failing assertion does not leak a running VM. Declare it
// after anything the VMs use (e.g. temporary disks), so that the VMs are gone before those are.
class ScopedHcsResources
{
public:
    ScopedHcsResources(std::vector<std::string> vms,
                       std::vector<std::string> endpoints,
                       std::vector<std::string> networks)
        : vms{std::move(vms)}, endpoints{std::move(endpoints)}, networks{std::move(networks)}
    {
        remove();
    }

    ~ScopedHcsResources()
    {
        remove();
    }

    ScopedHcsResources(const ScopedHcsResources&) = delete;
    ScopedHcsResources& operator=(const ScopedHcsResources&) = delete;

private:
    void remove() const noexcept
    {
        for (const auto& vm : vms)
        {
            if (hyperv::hcs::HcsSystemHandle vm_handle{nullptr};
                HCS().open_compute_system(vm, vm_handle))
                (void)HCS().terminate_compute_system(vm_handle);
        }
        for (const auto& endpoint : endpoints)
            (void)HCN().delete_endpoint(endpoint);
        for (const auto& network : networks)
            (void)HCN().delete_network(network);
    }

    std::vector<std::string> vms;
    std::vector<std::string> endpoints;
    std::vector<std::string> networks;
};

// Bring up one network + endpoint + guest on a given subnet/gateway, advertising a dotted hostname.
// vhdx/iso must already exist and outlive the VM. Returns the started VM handle.
hyperv::hcs::HcsSystemHandle boot_guest(const std::string& net_guid,
                                        const std::string& ep_guid,
                                        const std::string& vm_name,
                                        const std::string& mac,
                                        const std::string& subnet_cidr,
                                        const std::string& gateway,
                                        const std::filesystem::path& vhdx,
                                        const std::filesystem::path& iso)
{
    hyperv::hcn::CreateNetworkParameters net{};
    net.name = vm_name + "-net";
    net.guid = net_guid;
    net.flags = hyperv::hcn::HcnNetworkFlags::enable_dns_proxy |
                hyperv::hcn::HcnNetworkFlags::enable_dhcp_server;
    net.ipams = {hyperv::hcn::HcnIpam{
        hyperv::hcn::HcnIpamType::Static(),
        {hyperv::hcn::HcnSubnet{subnet_cidr, {hyperv::hcn::HcnRoute{gateway, "0.0.0.0/0", 0}}}}}};
    hyperv::hcn::CreateEndpointParameters ep{};
    ep.network_guid = net_guid;
    ep.endpoint_guid = ep_guid;
    (void)HCN().delete_endpoint(ep_guid);
    (void)HCN().delete_network(net_guid);
    EXPECT_TRUE(bool(HCN().create_network(net)));
    EXPECT_TRUE(bool(HCN().create_endpoint(ep)));

    hyperv::hcs::CreateComputeSystemParameters vm{};
    vm.name = vm_name;
    vm.processor_count = 1;
    vm.memory_size_mb = 512;
    hyperv::hcs::HcsNetworkAdapter na{};
    na.endpoint_guid = ep_guid;
    na.mac_address = mac;
    vm.network_adapters.push_back(na);
    vm.scsi_devices = {
        hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::VirtualDisk(),
                                   .name = "Primary disk",
                                   .path = vhdx},
        hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::Iso(),
                                   .name = "Cloud-init ISO",
                                   .path = iso,
                                   .read_only = true}};
    hyperv::hcs::HcsSystemHandle h{nullptr};
    EXPECT_TRUE(bool(HCS().create_compute_system(vm, h)));
    EXPECT_TRUE(HCS().grant_vm_access(vm_name, vhdx));
    EXPECT_TRUE(HCS().grant_vm_access(vm_name, iso));
    EXPECT_TRUE(bool(HCS().start_compute_system(h)));
    return h;
}
} // namespace

// Component level big bang integration tests for Hyper-V HCN/HCS + virtdisk API's.
// These tests ensure that the API's working together as expected.
struct HyperV_ComponentIntegrationTests : public ::testing::Test
{
    hyperv::hcs::HcsSystemHandle handle{nullptr};

    std::optional<decltype(make_tempfile_path(std::string{}))> temp_vhdx_path;

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

    void create_network_and_endpoint(
        const hyperv::hcn::CreateNetworkParameters& network_parameters,
        const hyperv::hcn::CreateEndpointParameters& endpoint_parameters)
    {
        const auto& [network_status, network_status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(network_status.success());
        ASSERT_TRUE(network_status_msg.empty());

        const auto& [endpoint_status,
                     endpoint_status_msg] = HCN().create_endpoint(endpoint_parameters);
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
    const auto network_parameters = make_network_parameters(
        hyperv::hcn::HcnNetworkFlags::enable_dhcp_server);
    const auto endpoint_parameters = make_endpoint_parameters(network_parameters,
                                                              "52-54-00-E9-36-7E");
    prepare_resources("multipass-hyperv-cit-vm", endpoint_parameters, network_parameters);

    temp_vhdx_path.emplace(make_tempfile_path(".vhdx"));
    const auto& temp_path = *temp_vhdx_path;
    const auto cloud_init_iso_path = std::filesystem::path{test_data_path} / "cloud-init" /
                                     "cloud-init.iso";

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

    const auto network_adapter = make_network_adapter(endpoint_parameters,
                                                      *endpoint_parameters.mac_address);
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
    for (auto attempts = 0; attempts < 720 && !neighbor_address; ++attempts)
    {
        neighbor_address = hyperv::management_ipv4_neighbor(network_parameters.guid,
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

    ASSERT_NO_FATAL_FAILURE(create_network_and_endpoint(network_parameters, endpoint_parameters));

    {
        const hyperv::hcs::CreateComputeSystemParameters parameters{.name = vm_name,
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
                                                ""};

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
    const auto create_vm_parameters = make_vm_parameters(
        {},
        {make_network_adapter(endpoint_parameters)});
    prepare_resources(create_vm_parameters.name, endpoint_parameters, network_parameters);

    temp_vhdx_path.emplace(make_tempfile_path(".vhdx"));
    const auto& temp_path = *temp_vhdx_path;

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
        const auto& [status, status_msg] = HCS().create_compute_system(create_vm_parameters,
                                                                       handle);
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

    temp_vhdx_path.emplace(make_tempfile_path(".vhdx"));
    const auto& temp_path = *temp_vhdx_path;

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

// End-to-end test: boot a real (minimal Alpine) guest on an ICS network that has a DNS
// suffix configured and verify that the host can resolve the guest by name to its leased IP.
//
// Two notes on what this exercises:
//   * The guest must advertise its hostname over DHCP (option 12) for the ICS DHCP/DNS proxy
//     to register it. Stock Alpine's busybox udhcpc does not, so we boot it with a seed (built
//     at run time, for a hostname unique to the run) that re-runs udhcpc with the hostname.
//   * The ICS DNS proxy registers leased hostnames under the machine-wide ICS domain
//     (HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\ICSDomain, default
//     "mshome.net"). That domain -- not the per-network HCN DNS suffix -- determines the FQDN
//     under which the host can resolve the guest. The per-network HCN DNS block is a
//     client-side hint pushed into the guest's resolver. We therefore resolve the guest under
//     the default ICS domain and assert it maps to the endpoint's actual leased address.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_hostname_resolution)
{
    constexpr auto dns_suffix = "mshome.net";
    const auto guest_hostname = unique_hostname("mp-host");
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
    const auto cloud_init_iso_path = make_tempfile_path(".iso");
    ASSERT_NO_FATAL_FAILURE(build_seed_iso(guest_hostname, cloud_init_iso_path));
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(vhdx_path)));

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

    const ScopedHcsResources resources{{vm_name},
                                       {endpoint_parameters.endpoint_guid},
                                       {network_parameters.guid}};

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

// End-to-end test of the custom DNS suffix design (NRPT only): an NRPT rule routing
// ".<suffix>" to the ICS DNS proxy, plus the ICS domain set to "<suffix>", gives an
// instance a clean custom suffix on the host -- with NO helper process, no hosts file,
// no mDNS, and no bundled resolver.
//
// Mechanism (NRPT routes; the ICS proxy is the authoritative source):
//   1. The ICS DNS proxy registers DHCP-leased guest names as "<hostname>.<ICSDomain>".
//      We set ICSDomain to the custom suffix (saved + restored by RAII), so the guest
//      is registered as "<hostname>.<suffix>" -- fully dynamic, owned by ICS.
//   2. An NRPT rule (the production NrptRule component) routes ".<suffix>" to the ICS
//      proxy on the gateway, so the host's system resolver answers
//      "<hostname>.<suffix>" with the guest's live leased IP.
//
// NRPT can only route a namespace to a server; it cannot rewrite/fabricate records. So
// the only NRPT-only way to a custom suffix is to have the authoritative server (the
// ICS proxy) hold the record under that suffix -- i.e. ICSDomain == suffix. The cost is
// the ICSDomain registry value (machine-global), which we save and restore.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_nrpt_resolution)
{
    constexpr auto custom_suffix = "multipass";
    const auto guest_hostname = unique_hostname("mp-nrpt");
    constexpr auto gateway_ip = "172.50.224.1"; // ICS proxy answers guest names here
    constexpr auto adapter_mac = "00-15-5D-9D-CF-71";
    const auto clean_fqdn = fmt::format("{}.{}", guest_hostname, custom_suffix); // the goal

    constexpr auto ics_domain_key = "HKLM:\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";

    // Save the current ICSDomain so it can be restored on scope exit.
    const auto saved_ics_domain = run_powershell(
        fmt::format("(Get-ItemProperty -Path '{}' -Name ICSDomain -EA SilentlyContinue).ICSDomain",
                    ics_domain_key));

    // Set ICSDomain to the custom suffix and (re)start SharedAccess so the ICS proxy
    // registers new guest leases under "<hostname>.<suffix>".
    (void)run_powershell(fmt::format("Set-ItemProperty -Path '{}' -Name ICSDomain -Value '{}'; "
                                     "Restart-Service SharedAccess -Force -EA SilentlyContinue",
                                     ics_domain_key,
                                     custom_suffix));

    // RAII: restore ICSDomain (and restart SharedAccess) on scope exit, even on failure.
    struct IcsDomainGuard
    {
        std::string key;
        std::string saved;
        ~IcsDomainGuard()
        {
            if (saved.empty())
                (void)run_powershell(fmt::format(
                    "Remove-ItemProperty -Path '{}' -Name ICSDomain -EA SilentlyContinue; "
                    "Restart-Service SharedAccess -Force -EA SilentlyContinue",
                    key));
            else
                (void)run_powershell(
                    fmt::format("Set-ItemProperty -Path '{}' -Name ICSDomain -Value '{}'; "
                                "Restart-Service SharedAccess -Force -EA SilentlyContinue",
                                key,
                                saved));
        }
    } ics_domain_guard{ics_domain_key, saved_ics_domain};

    // NRPT routes only ".multipass" to the ICS proxy (production NrptRule component).
    // RAII removes the rule on scope exit, including on assertion failure.
    hyperv::dns::NrptRule nrpt_rule{fmt::format(".{}", custom_suffix), {gateway_ip}};
    (void)run_powershell("Clear-DnsClientCache");

    hyperv::hcs::HcsSystemHandle handle{nullptr};

    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit-nrpt";
        network_parameters.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47491";
        network_parameters.flags = hyperv::hcn::HcnNetworkFlags::enable_dns_proxy |
                                   hyperv::hcn::HcnNetworkFlags::enable_dhcp_server;
        network_parameters.ipams = {hyperv::hcn::HcnIpam{
            hyperv::hcn::HcnIpamType::Static(),
            {hyperv::hcn::HcnSubnet{"172.50.224.0/20",
                                    {hyperv::hcn::HcnRoute{gateway_ip, "0.0.0.0/0", 0}}}}}};
        return network_parameters;
    }();

    const auto endpoint_parameters = [&network_parameters]() {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97031b";
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
    const auto cloud_init_iso_path = make_tempfile_path(".iso");
    ASSERT_NO_FATAL_FAILURE(build_seed_iso(guest_hostname, cloud_init_iso_path));
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(vhdx_path)));

    const auto vm_name = "multipass-hyperv-cit-nrpt-vm";

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

    const ScopedHcsResources resources{{vm_name},
                                       {endpoint_parameters.endpoint_guid},
                                       {network_parameters.guid}};

    {
        const auto& [status, status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(status.success()) << status_msg;
    }

    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status.success()) << status_msg;
    }
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

    (void)run_powershell("Clear-DnsClientCache");

    const auto resolve_system = [](const std::string& name) {
        return run_powershell(fmt::format(
            "(Resolve-DnsName -Name {} -Type A -DnsOnly -QuickTimeout "
            "-ErrorAction SilentlyContinue | Where-Object {{ $_.Type -eq 'A' }}).IPAddress "
            "-join ','",
            name));
    };

    std::string leased_ip{};
    std::string clean_resolved_ip{}; // custom suffix via the SYSTEM resolver (the goal)
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (std::chrono::steady_clock::now() < deadline)
    {
        leased_ip = neighbor_ip_for_mac(adapter_mac);
        if (!leased_ip.empty())
        {
            (void)run_powershell("Clear-DnsClientCache");
            clean_resolved_ip = resolve_system(clean_fqdn);
            if (clean_resolved_ip == leased_ip && !leased_ip.empty())
                break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    GTEST_LOG_(INFO) << "leased='" << leased_ip << "'  clean[" << clean_fqdn << " via system]='"
                     << clean_resolved_ip << "'";

    ASSERT_FALSE(leased_ip.empty()) << "Guest never obtained a DHCP lease.";
    EXPECT_EQ(clean_resolved_ip, leased_ip)
        << clean_fqdn << " did not resolve via the system resolver (NRPT -> ICS proxy).";

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
    EXPECT_TRUE(HCN().delete_endpoint(endpoint_parameters.endpoint_guid));
    EXPECT_TRUE(HCN().delete_network(network_parameters.guid));
}

// Custom DNS suffix with NO machine-global ICSDomain change and NO resolver process.
//
// The ICS DNS proxy registers a DHCP-leased guest as "<advertised-name>.<ICSDomain>",
// appending the (default, untouched) global ICSDomain verbatim. So if the guest advertises a
// DOTTED DHCP hostname "<host>.multipass", the proxy stores "<host>.multipass.mshome.net" --
// a fully dynamic record keyed off the live lease, with ICSDomain left at its default.
//
// The host then resolves the clean "<host>.multipass" via two PERMANENT, scoped settings (no
// time-windowing, no global ICSDomain mutation):
//   1. DNS-client multi-label suffix append (AppendToMultiLabelName + search suffix
//      "mshome.net"): a typed "<host>.multipass" that fails as-is is retried as
//      "<host>.multipass.mshome.net".
//   2. An NRPT rule routing the narrow namespace ".multipass.mshome.net" to the ICS proxy on
//      the gateway, which answers with the guest's live leased IP.
//
// This proves "<host>.multipass" resolves to the lease while the global ICSDomain stays
// "mshome.net" the entire time.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_fqdn_append_resolution)
{
    constexpr auto gateway_ip = "172.50.224.1"; // ICS proxy answers guest names here
    constexpr auto adapter_mac = "00-15-5D-9D-CF-71";
    // A UNIQUE dotted DHCP hostname per run. The ICS DNS proxy persists its name->IP
    // registrations across runs (and SharedAccess cannot be restarted while HNS pins it), so
    // reusing a hostname would resolve to a stale lease. A fresh label guarantees the proxy
    // holds no prior record for this name.
    const auto guest_hostname = unique_hostname("mp-fqdn") + ".multipass";
    const auto clean_fqdn = guest_hostname; // what the user types (the goal)
    // The name the ICS proxy actually stores (advertised + verbatim global ICSDomain).
    const auto append_suffix = std::string{"mshome.net"};
    const auto nrpt_namespace = ".multipass." + append_suffix; // narrow: .multipass.mshome.net

    constexpr auto global_ics_key = "HKLM:\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";

    // Capture the global ICSDomain up-front; this test must NOT change it.
    const auto global_ics_before = run_powershell(
        fmt::format("(Get-ItemProperty -Path '{}' -Name ICSDomain -EA SilentlyContinue).ICSDomain",
                    global_ics_key));

    // Host DNS-client config: append "mshome.net" to multi-label names. Saved + restored for
    // test hygiene (in production these are permanent install-time settings).
    const ScopedSuffixAppend suffix_append{append_suffix};

    // NRPT routes only ".multipass.mshome.net" to the ICS proxy (production NrptRule component).
    hyperv::dns::NrptRule nrpt_rule{nrpt_namespace, {gateway_ip}};
    (void)run_powershell("Clear-DnsClientCache");

    hyperv::hcs::HcsSystemHandle handle{nullptr};

    const auto network_parameters = []() {
        hyperv::hcn::CreateNetworkParameters network_parameters{};
        network_parameters.name = "multipass-hyperv-cit-fqdn";
        network_parameters.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47493";
        network_parameters.flags = hyperv::hcn::HcnNetworkFlags::enable_dns_proxy |
                                   hyperv::hcn::HcnNetworkFlags::enable_dhcp_server;
        network_parameters.ipams = {hyperv::hcn::HcnIpam{
            hyperv::hcn::HcnIpamType::Static(),
            {hyperv::hcn::HcnSubnet{"172.50.224.0/20",
                                    {hyperv::hcn::HcnRoute{gateway_ip, "0.0.0.0/0", 0}}}}}};
        return network_parameters;
    }();

    const auto endpoint_parameters = [&network_parameters]() {
        hyperv::hcn::CreateEndpointParameters endpoint_parameters{};
        endpoint_parameters.network_guid = network_parameters.guid;
        endpoint_parameters.endpoint_guid = "aee79cf9-54d1-4653-81fb-8110db97031b";
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

    // Build a NoCloud seed ISO advertising the unique dotted hostname at run time.
    const auto cloud_init_iso_path = make_tempfile_path(".iso");
    ASSERT_NO_FATAL_FAILURE(build_seed_iso(guest_hostname, cloud_init_iso_path));
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(vhdx_path)));

    const auto vm_name = "multipass-hyperv-cit-fqdn-vm";

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

    const ScopedHcsResources resources{{vm_name},
                                       {endpoint_parameters.endpoint_guid},
                                       {network_parameters.guid}};

    {
        const auto& [status, status_msg] = HCN().create_network(network_parameters);
        ASSERT_TRUE(status.success()) << status_msg;
    }
    {
        const auto& [status, status_msg] = HCN().create_endpoint(endpoint_parameters);
        ASSERT_TRUE(status.success()) << status_msg;
    }
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

    (void)run_powershell("Clear-DnsClientCache");

    const auto resolve_system = [](const std::string& name) {
        return run_powershell(fmt::format(
            "(Resolve-DnsName -Name {} -Type A -QuickTimeout "
            "-ErrorAction SilentlyContinue | Where-Object {{ $_.Type -eq 'A' }}).IPAddress "
            "-join ','",
            name));
    };

    std::string leased_ip{};
    std::string clean_resolved_ip{}; // clean "<host>.multipass" via the SYSTEM resolver (goal)
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (std::chrono::steady_clock::now() < deadline)
    {
        leased_ip = neighbor_ip_for_mac(adapter_mac);
        if (!leased_ip.empty())
        {
            (void)run_powershell("Clear-DnsClientCache");
            clean_resolved_ip = resolve_system(clean_fqdn);
            if (clean_resolved_ip == leased_ip && !leased_ip.empty())
                break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // The global ICSDomain must be unchanged by this test (proves no global mutation).
    const auto global_ics_after = run_powershell(
        fmt::format("(Get-ItemProperty -Path '{}' -Name ICSDomain -EA SilentlyContinue).ICSDomain",
                    global_ics_key));

    GTEST_LOG_(INFO) << "leased='" << leased_ip << "'  clean[" << clean_fqdn << " via system]='"
                     << clean_resolved_ip << "'  globalICSDomain before='" << global_ics_before
                     << "' after='" << global_ics_after << "'";

    EXPECT_EQ(global_ics_before, global_ics_after)
        << "Global ICSDomain changed -- this approach must not mutate it.";
    ASSERT_FALSE(leased_ip.empty()) << "Guest never obtained a DHCP lease.";
    EXPECT_EQ(clean_resolved_ip, leased_ip)
        << clean_fqdn << " did not resolve via the system resolver (append + NRPT -> ICS proxy).";

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
    EXPECT_TRUE(HCN().delete_endpoint(endpoint_parameters.endpoint_guid));
    EXPECT_TRUE(HCN().delete_network(network_parameters.guid));
}

// Multi-domain end-to-end proof: a SINGLE NRPT rule matching two suffixes (".zone1.mshome.net"
// and ".zone2.mshome.net") routes to the ICS proxy, and two guests -- each advertising a dotted
// hostname under a DIFFERENT zone -- resolve via the host's system resolver to their own leases.
//
// One network/proxy; both guests register as "<host>.<zone>.mshome.net". SuffixSearchList appends
// "mshome.net" so a typed "<host>.zoneN" becomes "<host>.zoneN.mshome.net" -> NRPT -> proxy. This
// proves the multi-suffix NrptRule (REG_MULTI_SZ) actually serves several domains at once.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_multi_zone_resolution)
{
    constexpr auto gateway_ip = "172.50.224.1"; // single ICS proxy for both guests
    constexpr auto mac1 = "00-15-5D-9D-CF-81";
    constexpr auto mac2 = "00-15-5D-9D-CF-82";
    const auto rng = [] {
        std::mt19937_64 r{std::random_device{}()};
        return static_cast<std::uint32_t>(r());
    };
    const auto host1 = fmt::format("mp-z1-{:08x}.zone1", rng());
    const auto host2 = fmt::format("mp-z2-{:08x}.zone2", rng());

    const ScopedSuffixAppend suffix_append{"mshome.net"};

    // A SINGLE rule, two suffixes -> the ICS proxy (the multi-suffix NrptRule under test).
    hyperv::dns::NrptRule nrpt_rule{
        std::vector<std::string>{".zone1.mshome.net", ".zone2.mshome.net"},
        {gateway_ip}};
    (void)run_powershell("Clear-DnsClientCache");

    auto net = []() {
        hyperv::hcn::CreateNetworkParameters n{};
        n.name = "multipass-hyperv-cit-multizone";
        n.guid = "b4d77a0e-2507-45f0-99aa-c638f3e47494";
        n.flags = hyperv::hcn::HcnNetworkFlags::enable_dns_proxy |
                  hyperv::hcn::HcnNetworkFlags::enable_dhcp_server;
        n.ipams = {hyperv::hcn::HcnIpam{
            hyperv::hcn::HcnIpamType::Static(),
            {hyperv::hcn::HcnSubnet{"172.50.224.0/20",
                                    {hyperv::hcn::HcnRoute{gateway_ip, "0.0.0.0/0", 0}}}}}};
        return n;
    }();

    auto make_ep = [&](const char* guid) {
        hyperv::hcn::CreateEndpointParameters e{};
        e.network_guid = net.guid;
        e.endpoint_guid = guid;
        return e;
    };
    const auto ep1 = make_ep("aee79cf9-54d1-4653-81fb-8110db970321");
    const auto ep2 = make_ep("aee79cf9-54d1-4653-81fb-8110db970322");

    const auto vhdx1 = make_tempfile_path(".vhdx"), vhdx2 = make_tempfile_path(".vhdx");
    reassemble_alpine_vhdx(vhdx1);
    reassemble_alpine_vhdx(vhdx2);
    const auto iso1 = make_tempfile_path(".iso"), iso2 = make_tempfile_path(".iso");
    const auto build_iso = [&](const std::string& host, const std::filesystem::path& iso) {
        return run_powershell(fmt::format("python '{}/cloud-init-fqdn-multipass/make_seed_iso.py' "
                                          "--hostname '{}' --output '{}' 2>&1",
                                          test_data_path,
                                          host,
                                          iso.string()));
    };
    const auto build_out1 = build_iso(host1, iso1);
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(iso1)))
        << build_out1;
    const auto build_out2 = build_iso(host2, iso2);
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(iso2)))
        << build_out2;

    auto make_vm = [&](const char* name,
                       const char* mac,
                       const char* ep_guid,
                       const std::filesystem::path& vhdx,
                       const std::filesystem::path& iso) {
        hyperv::hcs::CreateComputeSystemParameters vm{};
        vm.name = name;
        vm.processor_count = 1;
        vm.memory_size_mb = 512;
        hyperv::hcs::HcsNetworkAdapter na{};
        na.endpoint_guid = ep_guid;
        na.mac_address = mac;
        vm.network_adapters.push_back(na);
        vm.scsi_devices = {
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::VirtualDisk(),
                                       .name = "Primary disk",
                                       .path = vhdx},
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::Iso(),
                                       .name = "Cloud-init ISO",
                                       .path = iso,
                                       .read_only = true}};
        return vm;
    };
    const char* vm1n = "multipass-hyperv-cit-mz-vm1";
    const char* vm2n = "multipass-hyperv-cit-mz-vm2";

    const ScopedHcsResources resources{{vm1n, vm2n},
                                       {ep1.endpoint_guid, ep2.endpoint_guid},
                                       {net.guid}};
    ASSERT_TRUE(bool(HCN().create_network(net)));
    ASSERT_TRUE(bool(HCN().create_endpoint(ep1)));
    ASSERT_TRUE(bool(HCN().create_endpoint(ep2)));

    hyperv::hcs::HcsSystemHandle h1{nullptr}, h2{nullptr};
    ASSERT_TRUE(bool(
        HCS().create_compute_system(make_vm(vm1n, mac1, ep1.endpoint_guid.c_str(), vhdx1, iso1),
                                    h1)));
    ASSERT_TRUE(bool(
        HCS().create_compute_system(make_vm(vm2n, mac2, ep2.endpoint_guid.c_str(), vhdx2, iso2),
                                    h2)));
    ASSERT_TRUE(HCS().grant_vm_access(vm1n, vhdx1));
    ASSERT_TRUE(HCS().grant_vm_access(vm1n, iso1));
    ASSERT_TRUE(HCS().grant_vm_access(vm2n, vhdx2));
    ASSERT_TRUE(HCS().grant_vm_access(vm2n, iso2));
    ASSERT_TRUE(bool(HCS().start_compute_system(h1)));
    ASSERT_TRUE(bool(HCS().start_compute_system(h2)));

    const auto resolve_system = [](const std::string& name) {
        return run_powershell(fmt::format("(Resolve-DnsName -Name {} -Type A -QuickTimeout "
                                          "-ErrorAction SilentlyContinue | Where-Object {{ $_.Type "
                                          "-eq 'A' }}).IPAddress -join ','",
                                          name));
    };
    std::string lease1, lease2, r1, r2;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (std::chrono::steady_clock::now() < deadline)
    {
        lease1 = neighbor_ip_for_mac(mac1);
        lease2 = neighbor_ip_for_mac(mac2);
        if (!lease1.empty() && !lease2.empty())
        {
            (void)run_powershell("Clear-DnsClientCache");
            r1 = resolve_system(host1);
            r2 = resolve_system(host2);
            if (r1 == lease1 && r2 == lease2)
                break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    GTEST_LOG_(INFO) << host1 << "->'" << r1 << "' (lease '" << lease1 << "'); " << host2 << "->'"
                     << r2 << "' (lease '" << lease2 << "')";

    EXPECT_FALSE(lease1.empty());
    EXPECT_FALSE(lease2.empty());
    EXPECT_EQ(r1, lease1) << host1 << " did not resolve to its VM via the .zone1 suffix.";
    EXPECT_EQ(r2, lease2) << host2 << " did not resolve to its VM via the .zone2 suffix.";
    EXPECT_NE(lease1, lease2) << "distinct VMs must hold distinct leases.";

    (void)HCS().terminate_compute_system(h1);
    h1.reset();
    (void)HCS().terminate_compute_system(h2);
    h2.reset();
    EXPECT_TRUE(HCN().delete_endpoint(ep1.endpoint_guid));
    EXPECT_TRUE(HCN().delete_endpoint(ep2.endpoint_guid));
    EXPECT_TRUE(HCN().delete_network(net.guid));
}

// Two separate ICS networks, each owning a DIFFERENT suffix: a single multi-suffix NrptRule
// routes ".zone1.mshome.net" -> gw1 and ".zone2.mshome.net" -> gw2 (GenericDNSServers list). Each
// guest resolves to its own network's lease, proving multi-suffix works ACROSS networks.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_two_networks_distinct_zones)
{
    constexpr auto gw1 = "172.50.224.1";
    constexpr auto gw2 = "172.60.224.1";
    const auto rng = [] {
        std::mt19937_64 r{std::random_device{}()};
        return static_cast<std::uint32_t>(r());
    };
    const auto host1 = fmt::format("mp-na-{:08x}.zone1", rng());
    const auto host2 = fmt::format("mp-nb-{:08x}.zone2", rng());

    const ScopedSuffixAppend suffix_append{"mshome.net"};

    // ONE rule, two suffixes, two gateways (one per network).
    hyperv::dns::NrptRule nrpt{std::vector<std::string>{".zone1.mshome.net", ".zone2.mshome.net"},
                               {gw1, gw2}};
    (void)run_powershell("Clear-DnsClientCache");

    const auto v1 = make_tempfile_path(".vhdx"), v2 = make_tempfile_path(".vhdx"),
               i1 = make_tempfile_path(".iso"), i2 = make_tempfile_path(".iso");
    reassemble_alpine_vhdx(v1);
    reassemble_alpine_vhdx(v2);
    const auto mk = [&](const std::string& h, const std::filesystem::path& i) {
        return run_powershell(fmt::format("python '{}/cloud-init-fqdn-multipass/make_seed_iso.py' "
                                          "--hostname '{}' --output '{}' 2>&1",
                                          test_data_path,
                                          h,
                                          i.string()));
    };
    const auto o1 = mk(host1, i1);
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(i1))) << o1;
    const auto o2 = mk(host2, i2);
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(i2))) << o2;

    const ScopedHcsResources resources{
        {"mp-net1-vm", "mp-net2-vm"},
        {"aee79cf9-54d1-4653-81fb-8110db970331", "aee79cf9-54d1-4653-81fb-8110db970332"},
        {"b4d77a0e-2507-45f0-99aa-c638f3e47495", "b4d77a0e-2507-45f0-99aa-c638f3e47496"}};
    auto h1 = boot_guest("b4d77a0e-2507-45f0-99aa-c638f3e47495",
                         "aee79cf9-54d1-4653-81fb-8110db970331",
                         "mp-net1-vm",
                         "00-15-5D-9D-CF-91",
                         "172.50.224.0/20",
                         gw1,
                         v1,
                         i1);
    auto h2 = boot_guest("b4d77a0e-2507-45f0-99aa-c638f3e47496",
                         "aee79cf9-54d1-4653-81fb-8110db970332",
                         "mp-net2-vm",
                         "00-15-5D-9D-CF-92",
                         "172.60.224.0/20",
                         gw2,
                         v2,
                         i2);

    const auto resolve = [](const std::string& n) {
        return run_powershell(
            fmt::format("(Resolve-DnsName -Name {} -Type A -QuickTimeout -EA SilentlyContinue | "
                        "Where-Object {{ $_.Type -eq 'A' }}).IPAddress -join ','",
                        n));
    };
    std::string l1, l2, r1, r2;
    const auto dl = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (std::chrono::steady_clock::now() < dl)
    {
        l1 = neighbor_ip_for_mac("00-15-5D-9D-CF-91", "172.50.*");
        l2 = neighbor_ip_for_mac("00-15-5D-9D-CF-92", "172.60.*");
        if (!l1.empty() && !l2.empty())
        {
            (void)run_powershell("Clear-DnsClientCache");
            r1 = resolve(host1);
            r2 = resolve(host2);
            if (r1 == l1 && r2 == l2)
                break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    GTEST_LOG_(INFO) << host1 << "->'" << r1 << "'(net1 '" << l1 << "'); " << host2 << "->'" << r2
                     << "'(net2 '" << l2 << "')";
    EXPECT_FALSE(l1.empty());
    EXPECT_FALSE(l2.empty());
    EXPECT_EQ(r1, l1) << host1 << " did not resolve to net1 guest.";
    EXPECT_EQ(r2, l2) << host2 << " did not resolve to net2 guest.";
    (void)HCS().terminate_compute_system(h1);
    h1.reset();
    (void)HCS().terminate_compute_system(h2);
    h2.reset();
    EXPECT_TRUE(HCN().delete_endpoint("aee79cf9-54d1-4653-81fb-8110db970331"));
    EXPECT_TRUE(HCN().delete_endpoint("aee79cf9-54d1-4653-81fb-8110db970332"));
    EXPECT_TRUE(HCN().delete_network("b4d77a0e-2507-45f0-99aa-c638f3e47495"));
    EXPECT_TRUE(HCN().delete_network("b4d77a0e-2507-45f0-99aa-c638f3e47496"));
}

// ONE domain assigned to TWO networks at once: both guests advertise ".zone1" on separate networks
// and a single rule routes ".zone1.mshome.net" to BOTH gateways. Both resolve simultaneously --
// the suffix is not bound to a single network.
TEST_F(HyperV_ComponentIntegrationTests, dns_suffix_one_zone_two_networks)
{
    constexpr auto gw1 = "172.50.224.1";
    constexpr auto gw2 = "172.60.224.1";
    const auto rng = [] {
        std::mt19937_64 r{std::random_device{}()};
        return static_cast<std::uint32_t>(r());
    };
    const auto host1 = fmt::format("mp-sa-{:08x}.zone1", rng());
    const auto host2 = fmt::format("mp-sb-{:08x}.zone1", rng());

    const ScopedSuffixAppend suffix_append{"mshome.net"};

    // ONE suffix routed to BOTH gateways -- the same domain spans both networks.
    hyperv::dns::NrptRule nrpt{".zone1.mshome.net", {gw1, gw2}};
    (void)run_powershell("Clear-DnsClientCache");

    const auto v1 = make_tempfile_path(".vhdx"), v2 = make_tempfile_path(".vhdx"),
               i1 = make_tempfile_path(".iso"), i2 = make_tempfile_path(".iso");
    reassemble_alpine_vhdx(v1);
    reassemble_alpine_vhdx(v2);
    const auto mk = [&](const std::string& h, const std::filesystem::path& i) {
        return run_powershell(fmt::format("python '{}/cloud-init-fqdn-multipass/make_seed_iso.py' "
                                          "--hostname '{}' --output '{}' 2>&1",
                                          test_data_path,
                                          h,
                                          i.string()));
    };
    const auto o1 = mk(host1, i1);
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(i1))) << o1;
    const auto o2 = mk(host2, i2);
    ASSERT_TRUE(std::filesystem::exists(static_cast<const std::filesystem::path&>(i2))) << o2;

    const ScopedHcsResources resources{
        {"mp-zn1-vm", "mp-zn2-vm"},
        {"aee79cf9-54d1-4653-81fb-8110db970341", "aee79cf9-54d1-4653-81fb-8110db970342"},
        {"b4d77a0e-2507-45f0-99aa-c638f3e47497", "b4d77a0e-2507-45f0-99aa-c638f3e47498"}};
    auto h1 = boot_guest("b4d77a0e-2507-45f0-99aa-c638f3e47497",
                         "aee79cf9-54d1-4653-81fb-8110db970341",
                         "mp-zn1-vm",
                         "00-15-5D-9D-CF-A1",
                         "172.50.224.0/20",
                         gw1,
                         v1,
                         i1);
    auto h2 = boot_guest("b4d77a0e-2507-45f0-99aa-c638f3e47498",
                         "aee79cf9-54d1-4653-81fb-8110db970342",
                         "mp-zn2-vm",
                         "00-15-5D-9D-CF-A2",
                         "172.60.224.0/20",
                         gw2,
                         v2,
                         i2);

    const auto resolve = [](const std::string& n) {
        return run_powershell(
            fmt::format("(Resolve-DnsName -Name {} -Type A -QuickTimeout -EA SilentlyContinue | "
                        "Where-Object {{ $_.Type -eq 'A' }}).IPAddress -join ','",
                        n));
    };
    std::string l1, l2, r1, r2;
    const auto dl = std::chrono::steady_clock::now() + std::chrono::seconds(180);
    while (std::chrono::steady_clock::now() < dl)
    {
        l1 = neighbor_ip_for_mac("00-15-5D-9D-CF-A1", "172.50.*");
        l2 = neighbor_ip_for_mac("00-15-5D-9D-CF-A2", "172.60.*");
        if (!l1.empty() && !l2.empty())
        {
            (void)run_powershell("Clear-DnsClientCache");
            r1 = resolve(host1);
            r2 = resolve(host2);
            if (r1 == l1 && r2 == l2)
                break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    GTEST_LOG_(INFO) << host1 << "->'" << r1 << "'(net1 '" << l1 << "'); " << host2 << "->'" << r2
                     << "'(net2 '" << l2 << "')";
    EXPECT_FALSE(l1.empty());
    EXPECT_FALSE(l2.empty());
    EXPECT_EQ(r1, l1) << host1 << " (.zone1 on net1) did not resolve.";
    EXPECT_EQ(r2, l2) << host2 << " (.zone1 on net2) did not resolve.";
    EXPECT_NE(l1, l2) << "guests on different networks must hold distinct leases.";
    (void)HCS().terminate_compute_system(h1);
    h1.reset();
    (void)HCS().terminate_compute_system(h2);
    h2.reset();
    EXPECT_TRUE(HCN().delete_endpoint("aee79cf9-54d1-4653-81fb-8110db970341"));
    EXPECT_TRUE(HCN().delete_endpoint("aee79cf9-54d1-4653-81fb-8110db970342"));
    EXPECT_TRUE(HCN().delete_network("b4d77a0e-2507-45f0-99aa-c638f3e47497"));
    EXPECT_TRUE(HCN().delete_network("b4d77a0e-2507-45f0-99aa-c638f3e47498"));
}

} // namespace multipass::test
