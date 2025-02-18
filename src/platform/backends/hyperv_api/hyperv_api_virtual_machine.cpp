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
#include "hcs/hyperv_hcs_compute_system_state.h"
#include <multipass/virtual_machine_description.h>

#include <stdexcept>

namespace multipass::hyperv
{

inline auto mac2uuid(std::string mac_addr)
{
    // 00000000-0000-0001-8000-0123456789ab
    mac_addr.erase(std::remove(mac_addr.begin(), mac_addr.end(), ':'), mac_addr.end());
    mac_addr.erase(std::remove(mac_addr.begin(), mac_addr.end(), '-'), mac_addr.end());
    constexpr auto format_str = "00000000-0000-0001-8000-{}";
    return fmt::format(format_str, mac_addr);
}

struct InvalidAPIPointerException : std::runtime_error
{
    InvalidAPIPointerException() : std::runtime_error("")
    {
        assert(0);
    }
};

struct CreateComputeSystemException : std::runtime_error
{
    CreateComputeSystemException() : std::runtime_error("")
    {
        assert(0);
    }
};

HyperVAPIVirtualMachine::HyperVAPIVirtualMachine(

    unique_hcn_wrapper_t hcn_w,
    unique_hcs_wrapper_t hcs_w,
    unique_virtdisk_wrapper_t virtdisk_w,
    const VirtualMachineDescription& desc,
    class VMStatusMonitor& monitor,
    const SSHKeyProvider& key_provider,
    const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir},
      description(desc),
      hcn(std::move(hcn_w)),
      hcs(std::move(hcs_w)),
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

    // Check if the VM already exist
    const auto result = hcs->get_compute_system_state(vm_name);
    if (result)
    {
        // VM already exist. Translate the VM state
        // hcs::compute_system_state_from_string(result.status_msg);
    }
    else
    {
        // Create the VM from scratch.
        const auto ccs_params = [&desc]() {
            hcs::CreateComputeSystemParameters ccs_params{};
            ccs_params.name = desc.vm_name;
            ccs_params.memory_size_mb = desc.mem_size.in_megabytes();
            ccs_params.processor_count = desc.num_cores;
            ccs_params.cloudinit_iso_path = desc.cloud_init_iso.toStdString();
            ccs_params.vhdx_path = desc.image.image_path.toStdString();

            hcs::AddEndpointParameters default_endpoint_params{};
            default_endpoint_params.nic_mac_address = desc.default_mac_address;
            // make the UUID deterministic so we can query the endpoint with a MAC address
            // if needed.
            default_endpoint_params.endpoint_guid = mac2uuid(desc.default_mac_address);
            default_endpoint_params.target_compute_system_name = ccs_params.name;
            ccs_params.endpoints.push_back(default_endpoint_params);

            for (const auto& v : desc.extra_interfaces)
            {
                hcs::AddEndpointParameters endpoint_params{};
                endpoint_params.nic_mac_address = v.mac_address;
                endpoint_params.endpoint_guid = mac2uuid(v.mac_address);
                endpoint_params.target_compute_system_name = ccs_params.name;
                ccs_params.endpoints.push_back(endpoint_params);
            }
            return ccs_params;
        }();

        if (const auto create_result = hcs->create_compute_system(ccs_params); !create_result)
        {

            throw CreateComputeSystemException{};
        }
        else
        {
            // Create successful
        }
    }
}

// HyperVAPIVirtualMachine::HyperVAPIVirtualMachine(const std::string& source_vm_name,
//                                                  const multipass::VMSpecs& src_vm_specs,
//                                                  const VirtualMachineDescription& desc,
//                                                  VMStatusMonitor& monitor,
//                                                  const SSHKeyProvider& key_provider,
//                                                  const Path& dest_instance_dir)
// {
// }

void HyperVAPIVirtualMachine::start()
{
    const auto& [status, status_msg] = hcs->start_compute_system(vm_name);
}
void HyperVAPIVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    const auto& [status, status_msg] = hcs->shutdown_compute_system(vm_name);
}

void HyperVAPIVirtualMachine::suspend()
{
    const auto& [status, status_msg] = hcs->pause_compute_system(vm_name);
}
HyperVAPIVirtualMachine::State HyperVAPIVirtualMachine::current_state()
{
    throw std::runtime_error{"Not yet implemented"};
}
int HyperVAPIVirtualMachine::ssh_port()
{
    constexpr auto kDefaultSSHPort = 22;
    return kDefaultSSHPort;
}
std::string HyperVAPIVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    throw std::runtime_error{"Not yet implemented"};
}
std::string HyperVAPIVirtualMachine::ssh_username()
{
    return description.ssh_username;
}
std::string HyperVAPIVirtualMachine::management_ipv4()
{
    throw std::runtime_error{"Not yet implemented"};
}
std::string HyperVAPIVirtualMachine::ipv6()
{
    return {};
}
void HyperVAPIVirtualMachine::ensure_vm_is_running()
{
    throw std::runtime_error{"Not yet implemented"};
}
void HyperVAPIVirtualMachine::update_state()
{
    throw std::runtime_error{"Not yet implemented"};
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