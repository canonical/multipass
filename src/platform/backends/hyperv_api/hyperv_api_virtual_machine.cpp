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

#include "hyperv_api_virtual_machine.h"
#include "hcn/hyperv_hcn_create_endpoint_params.h"
#include "hcs/hyperv_hcs_compute_system_state.h"
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>
#include <shared/shared_backend_utils.h>

#include <fmt/xchar.h>

#include <stdexcept>

#include <WS2tcpip.h>

namespace
{

/**
 * Category for the log messages.
 */
constexpr auto kLogCategory = "HyperV-Virtual-Machine";

namespace mpl = multipass::logging;
using lvl = mpl::Level;

inline auto mac2uuid(std::string mac_addr)
{
    // 00000000-0000-0001-8000-0123456789ab
    mac_addr.erase(std::remove(mac_addr.begin(), mac_addr.end(), ':'), mac_addr.end());
    mac_addr.erase(std::remove(mac_addr.begin(), mac_addr.end(), '-'), mac_addr.end());
    constexpr auto format_str = "db4bdbf0-dc14-407f-9780-{}";
    return fmt::format(format_str, mac_addr);
}
// db4bdbf0-dc14-407f-9780-f528c59b0c58
inline auto replace_colon_with_dash(std::string& addr)
{
    if (addr.empty())
        return;
    std::replace(addr.begin(), addr.end(), ':', '-');
}

auto resolve_ip_addresses(const std::string& hostname)
{
    static auto wsa_context = [] {
        struct wsa_init_wrapper
        {
            wsa_init_wrapper() : wsa_data{}, wsa_init_success(WSAStartup(MAKEWORD(2, 2), &wsa_data))
            {
                if (!wsa_init_success)
                {
                    mpl::log(lvl::error, kLogCategory, "resolve_and_memoize_ip_addresses() > WSAStartup failed!");
                }
            }
            ~wsa_init_wrapper()
            {
                /**
                 * https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsacleanup
                 * There must be a call to WSACleanup for each successful call to WSAStartup.
                 * Only the final WSACleanup function call performs the actual cleanup.
                 * The preceding calls simply decrement an internal reference count in the WS2_32.DLL.
                 */
                if (wsa_init_success)
                {
                    WSACleanup();
                }
            }
            WSADATA wsa_data{};
            const bool wsa_init_success{false};
        };

        return wsa_init_wrapper{};
    }();

    // Wrap the raw addrinfo pointer so it's always destroyed properly.
    const auto& [result, addr_info] = [&]() {
        struct addrinfo* result = {nullptr};
        struct addrinfo hints{};
        const auto r = getaddrinfo(hostname.c_str(), nullptr, nullptr, &result);
        return std::make_pair(r, std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{result, freeaddrinfo});
    }();

    std::vector<std::string> ipv4{}, ipv6{};
    if (result == 0)
    {
        assert(addr_info.get());
        for (auto ptr = addr_info.get(); ptr != nullptr; ptr = addr_info->ai_next)
        {
            switch (ptr->ai_family)
            {
            case AF_INET:
            {
                const auto sockaddr_ipv4 = reinterpret_cast<struct sockaddr_in*>(ptr->ai_addr);
                char addr[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), addr, sizeof(addr));
                ipv4.push_back(addr);
            }
            break;
            case AF_INET6:
            {
                const auto sockaddr_ipv6 = reinterpret_cast<LPSOCKADDR>(ptr->ai_addr);
                char addr[INET6_ADDRSTRLEN] = {};
                inet_ntop(AF_INET6, &(sockaddr_ipv6->sa_data), addr, sizeof(addr));
                ipv6.push_back(addr);
            }
            break;
            default:
                continue;
            }
        }
    }
    return std::make_pair(ipv4, ipv6);
}
} // namespace

namespace multipass::hyperv
{

struct ExceptionFormatter : std::runtime_error
{
    template <typename... Args>
    ExceptionFormatter(fmt::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error{fmt::format(fmt, std::forward<Args>(args)...)}
    {
    }
    ExceptionFormatter() : std::runtime_error("")
    {
    }
};

struct InvalidAPIPointerException : ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

struct CreateComputeSystemException : ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

struct ComputeSystemStateException : ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

struct CreateEndpointException : ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

struct GrantVMAccessException : ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

HyperVAPIVirtualMachine::HyperVAPIVirtualMachine(unique_hcs_wrapper_t hcs_w,
                                                 unique_hcn_wrapper_t hcn_w,
                                                 unique_virtdisk_wrapper_t virtdisk_w,
                                                 const std::string& network_guid,
                                                 const VirtualMachineDescription& desc,
                                                 class VMStatusMonitor& monitor,
                                                 const SSHKeyProvider& key_provider,
                                                 const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir},
      description(desc),
      hcs(std::move(hcs_w)),
      hcn(std::move(hcn_w)),
      monitor(monitor),
      virtdisk(std::move(virtdisk_w))
{
    // Verify that the given API wrappers are not null
    {
        const void* api_ptrs[] = {hcs.get(), hcn.get(), virtdisk.get()};
        if (std::any_of(std::begin(api_ptrs), std::end(api_ptrs), [](const void* ptr) { return nullptr == ptr; }))
        {
            throw InvalidAPIPointerException{};
        }
    }

    hcs::ComputeSystemState cs_state{hcs::ComputeSystemState::unknown};

    {
        // Check if the VM already exist
        const auto result = hcs->get_compute_system_state(vm_name, cs_state);

        if (E_INVALIDARG == static_cast<HRESULT>(result.code))
        {

            const auto endpoint_params = [&desc, &network_guid]() {
                std::vector<hcn::CreateEndpointParameters> endpoint_params{};
                endpoint_params.emplace_back(
                    hcn::CreateEndpointParameters{network_guid, mac2uuid(desc.default_mac_address)});

                // TODO: Figure out what to do with the "extra interfaces"
                // for (const auto& v : desc.extra_interfaces)
                // {
                //     endpoint_params.emplace_back(network_guid, mac2uuid(v.mac_address));
                // }
                return endpoint_params;
            }();

            for (const auto& endpoint : endpoint_params)
            {
                // There might be remnants from an old VM, remove the endpoint if exist before
                // creating it again.
                if (hcn->delete_endpoint(endpoint.endpoint_guid))
                {
                    // TODO: log
                }
                if (const auto& [status, msg] = hcn->create_endpoint(endpoint); !status)
                {
                    throw CreateEndpointException{};
                }
            }

            // E_INVALIDARG means there's no such VM.
            // Create the VM from scratch.
            const auto ccs_params = [&desc, &endpoint_params]() {
                hcs::CreateComputeSystemParameters ccs_params{};
                ccs_params.name = desc.vm_name;
                ccs_params.memory_size_mb = desc.mem_size.in_megabytes();
                ccs_params.processor_count = desc.num_cores;
                ccs_params.cloudinit_iso_path = desc.cloud_init_iso.toStdString();
                ccs_params.vhdx_path = desc.image.image_path.toStdString();

                hcs::AddEndpointParameters default_endpoint_params{};
                default_endpoint_params.nic_mac_address = desc.default_mac_address;
                // Hyper-V API does not like colons. Ensure that the MAC is separated
                // with dash instead of colon.
                replace_colon_with_dash(default_endpoint_params.nic_mac_address);
                // make the UUID deterministic so we can query the endpoint with a MAC address
                // if needed.
                default_endpoint_params.endpoint_guid = mac2uuid(desc.default_mac_address);
                default_endpoint_params.target_compute_system_name = ccs_params.name;
                ccs_params.endpoints.push_back(default_endpoint_params);

                // TODO: Figure out what to do with the "extra interfaces"
                // for (const auto& v : desc.extra_interfaces)
                // {
                //     hcs::AddEndpointParameters endpoint_params{};
                //     endpoint_params.nic_mac_address = v.mac_address;
                //     endpoint_params.endpoint_guid = mac2uuid(v.mac_address);
                //     endpoint_params.target_compute_system_name = ccs_params.name;
                //     ccs_params.endpoints.push_back(endpoint_params);
                // }
                return ccs_params;
            }();

            if (const auto create_result = hcs->create_compute_system(ccs_params); !create_result)
            {
                fmt::print(L"{}", create_result.status_msg);
                throw CreateComputeSystemException{};
            }

            // Grant access to the VHDX and the cloud-init ISO files.
            const auto grant_paths = {ccs_params.cloudinit_iso_path, ccs_params.vhdx_path};

            for (const auto& path : grant_paths)
            {
                if (!hcs->grant_vm_access(ccs_params.name, path))
                {
                    throw GrantVMAccessException{"Could not grant access to VM `{}` for the path `{}`",
                                                 ccs_params.name,
                                                 path};
                }
            }
        }
    }
    // Reflect compute system's state
    set_state(fetch_state_from_api());
    update_state();
}

// HyperVAPIVirtualMachine::HyperVAPIVirtualMachine(const std::string& source_vm_name,
//                                                  const multipass::VMSpecs& src_vm_specs,
//                                                  const VirtualMachineDescription& desc,
//                                                  VMStatusMonitor& monitor,
//                                                  const SSHKeyProvider& key_provider,
//                                                  const Path& dest_instance_dir)
// {
// }

void HyperVAPIVirtualMachine::set_state(hcs::ComputeSystemState compute_system_state)
{
    const auto prev_state = state;
    switch (compute_system_state)
    {
    case hcs::ComputeSystemState::created:
        state = State::off;
        break;
    case hcs::ComputeSystemState::paused:
        state = State::suspended;
        break;
    case hcs::ComputeSystemState::running:
        state = State::running;
        break;
    case hcs::ComputeSystemState::saved_as_template:
    case hcs::ComputeSystemState::stopped:
        state = State::stopped;
        break;
    case hcs::ComputeSystemState::unknown:
        state = State::unknown;
        break;
    }

    if (state != prev_state)
    {
        mpl::log(lvl::info,
                 kLogCategory,
                 fmt::format("set_state() > VM {} state changed from {} to {}",
                             vm_name,
                             fmt::underlying(prev_state),
                             fmt::underlying(state)));
    }
}

void HyperVAPIVirtualMachine::start()
{
    state = VirtualMachine::State::starting;
    update_state();
    // Resume and start are the same thing in Multipass terms
    // Try to determine whether we need to resume or start here.
    const auto& [status, status_msg] = [&] {
        // Fetch the latest state value.
        if (fetch_state_from_api() == hcs::ComputeSystemState::paused)
        {
            return hcs->resume_compute_system(vm_name);
        }
        return hcs->start_compute_system(vm_name);
    }();

    // TODO: Check status message here

    // Maybe wait until SSH is up with timeout
    std::this_thread::sleep_for(std::chrono::seconds{60});
    set_state(fetch_state_from_api());
    update_state();
}
void HyperVAPIVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{

    switch (shutdown_policy)
    {
    case ShutdownPolicy::Powerdown:
        // We have to rely on SSH for now since we don't have the means to
        // run a guest action from host natively.
        // FIXME: Find a way to trigger ACPI shutdown, host-to-guest syscall,
        // some way to signal "vmwp.exe" for a graceful shutdown, sysrq via console
        // or other "direct" means to trigger the shutdown.
        ssh_exec("sudo shutdown -h now");
        drop_ssh_session();
        break;
    case ShutdownPolicy::Halt:
    case ShutdownPolicy::Poweroff:
        drop_ssh_session();
        // These are non-graceful variants. Just terminate the system immediately.
        hcs->terminate_compute_system(vm_name);
        break;
    }

    state = State::off;
    update_state();
}

void HyperVAPIVirtualMachine::suspend()
{
    const auto& [status, status_msg] = hcs->pause_compute_system(vm_name);
    set_state(fetch_state_from_api());
    update_state();
}

HyperVAPIVirtualMachine::State HyperVAPIVirtualMachine::current_state()
{
    return state;
}
int HyperVAPIVirtualMachine::ssh_port()
{
    constexpr auto kDefaultSSHPort = 22;
    return kDefaultSSHPort;
}
std::string HyperVAPIVirtualMachine::ssh_hostname(std::chrono::milliseconds /*timeout*/)
{
    return fmt::format("{}.mshome.net", vm_name);
}
std::string HyperVAPIVirtualMachine::ssh_username()
{
    return description.ssh_username;
}

std::string HyperVAPIVirtualMachine::management_ipv4()
{
    const auto& [ipv4, _] = resolve_ip_addresses(ssh_hostname({}).c_str());
    if (ipv4.empty())
    {
        // TODO: Log
        return {};
    }

    const auto result = *ipv4.begin();

    mpl::log(lvl::info, kLogCategory, fmt::format("management_ipv4() > IP address is `{}`", result));
    // Prefer the first one
    return result;
}
std::string HyperVAPIVirtualMachine::ipv6()
{
    const auto& [_, ipv6] = resolve_ip_addresses(ssh_hostname({}).c_str());
    if (ipv6.empty())
    {
        // TODO: Log
        return {};
    }
    // Prefer the first one
    return *ipv6.begin();
}
void HyperVAPIVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] { return state != State::off; };
    multipass::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance shutdown during start");
}
void HyperVAPIVirtualMachine::update_state()
{
    monitor.persist_state_for(vm_name, state);
}

hcs::ComputeSystemState HyperVAPIVirtualMachine::fetch_state_from_api()
{
    hcs::ComputeSystemState compute_system_state{hcs::ComputeSystemState::unknown};
    const auto result = hcs->get_compute_system_state(vm_name, compute_system_state);
    return compute_system_state;
}

void HyperVAPIVirtualMachine::update_cpus(int num_cores)
{
    throw std::runtime_error{"Not yet implemented"};
}
void HyperVAPIVirtualMachine::resize_memory(const MemorySize& new_size)
{
    const auto& [status, status_msg] = hcs->resize_memory(vm_name, new_size.in_megabytes());
}
void HyperVAPIVirtualMachine::resize_disk(const MemorySize& new_size)
{
    throw std::runtime_error{"Not yet implemented"};
}
void HyperVAPIVirtualMachine::add_network_interface(int index,
                                                    const std::string& default_mac_addr,
                                                    const NetworkInterface& extra_interface)
{
    throw std::runtime_error{"Not yet implemented"};
}
std::unique_ptr<MountHandler> HyperVAPIVirtualMachine::make_native_mount_handler(const std::string& target,
                                                                                 const VMMount& mount)
{
    throw std::runtime_error{"Not yet implemented"};
}

} // namespace multipass::hyperv