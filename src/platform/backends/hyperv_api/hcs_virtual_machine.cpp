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

#include <hyperv_api/hcs_virtual_machine.h>

#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper_interface.h>
#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_event_type.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper_interface.h>
#include <hyperv_api/hcs_plan9_mount_handler.h>
#include <hyperv_api/hcs_virtual_machine_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_snapshot.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper_interface.h>

#include <multipass/exceptions/formatted_exception_base.h>
#include <multipass/platform_win.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>
#include <shared/shared_backend_utils.h>
#include <shared/windows/smb_mount_handler.h>

#include <fmt/xchar.h>

#include <stdexcept>

#include <WS2tcpip.h>

namespace
{

/**
 * Category for the log messages.
 */
constexpr static auto kLogCategory = "HyperV-Virtual-Machine";
constexpr static auto kVhdxFileName = "current.vhdx";
constexpr static auto kDefaultSSHPort = 22;

namespace mpl = multipass::logging;
namespace mpp = multipass::platform;
using lvl = mpl::Level;

inline auto mac2uuid(std::string mac_addr)
{
    // 00000000-0000-0001-8000-0123456789ab
    mac_addr.erase(std::remove(mac_addr.begin(), mac_addr.end(), ':'), mac_addr.end());
    mac_addr.erase(std::remove(mac_addr.begin(), mac_addr.end(), '-'), mac_addr.end());
    constexpr static auto format_str = "db4bdbf0-dc14-407f-9780-{}";
    return fmt::format(format_str, mac_addr);
}
// db4bdbf0-dc14-407f-9780-f528c59b0c58
inline auto replace_colon_with_dash(std::string& addr)
{
    if (addr.empty())
        return;
    std::replace(addr.begin(), addr.end(), ':', '-');
}

/**
 * Perform a DNS resolve of @p hostname to obtain IPv4/IPv6
 * address(es) associated with it.
 *
 * @param [in] hostname Hostname to resolve
 * @return Vector of IPv4/IPv6 addresses
 */
auto resolve_ip_addresses(const std::string& hostname)
{
    const static mpp::wsa_init_wrapper wsa_context{};

    std::vector<std::string> ipv4{}, ipv6{};
    mpl::trace(kLogCategory,
               "resolve_ip_addresses() -> resolve being called for hostname `{}`",
               hostname);

    if (!wsa_context)
    {
        mpl::error(kLogCategory, "resolve_ip_addresses() -> WSA not initialized! `{}`", hostname);
        return std::make_pair(ipv4, ipv6);
    }

    // Wrap the raw addrinfo pointer so it's always destroyed properly.
    const auto& [result, addr_info] = [&]() {
        struct addrinfo* result = {nullptr};
        struct addrinfo hints
        {
        };
        const auto r = getaddrinfo(hostname.c_str(), nullptr, nullptr, &result);
        return std::make_pair(
            r,
            std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{result, freeaddrinfo});
    }();

    if (result == 0)
    {
        assert(addr_info.get());
        for (auto ptr = addr_info.get(); ptr != nullptr; ptr = addr_info->ai_next)
        {
            switch (ptr->ai_family)
            {
            case AF_INET:
            {
                constexpr static auto kSockaddrInSize =
                    sizeof(std::remove_pointer_t<LPSOCKADDR_IN>);
                if (ptr->ai_addrlen >= kSockaddrInSize)
                {
                    const auto sockaddr_ipv4 = reinterpret_cast<LPSOCKADDR_IN>(ptr->ai_addr);
                    char addr[INET_ADDRSTRLEN] = {};
                    inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), addr, sizeof(addr));
                    ipv4.push_back(addr);
                    break;
                }

                mpl::error(kLogCategory,
                           "resolve_ip_addresses() -> anomaly: received {} bytes of IPv4 address "
                           "data while expecting {}!",
                           ptr->ai_addrlen,
                           kSockaddrInSize);
            }
            break;
            case AF_INET6:
            {
                constexpr static auto kSockaddrIn6Size =
                    sizeof(std::remove_pointer_t<LPSOCKADDR_IN6>);
                if (ptr->ai_addrlen >= kSockaddrIn6Size)
                {
                    const auto sockaddr_ipv6 = reinterpret_cast<LPSOCKADDR_IN6>(ptr->ai_addr);
                    char addr[INET6_ADDRSTRLEN] = {};
                    inet_ntop(AF_INET6, &(sockaddr_ipv6->sin6_addr), addr, sizeof(addr));
                    ipv6.push_back(addr);
                    break;
                }
                mpl::error(kLogCategory,
                           "resolve_ip_addresses() -> anomaly: received {} bytes of IPv6 address "
                           "data while expecting {}!",
                           ptr->ai_addrlen,
                           kSockaddrIn6Size);
            }
            break;
            default:
                continue;
            }
        }
    }

    mpl::trace(kLogCategory,
               "resolve_ip_addresses() -> hostname: {} resolved to : (v4: {}, v6: {})",
               hostname,
               fmt::join(ipv4, ","),
               fmt::join(ipv6, ","));

    return std::make_pair(ipv4, ipv6);
}
} // namespace

namespace multipass::hyperv
{

HCSVirtualMachine::HCSVirtualMachine(hcs_sptr_t hcs_w,
                                     hcn_sptr_t hcn_w,
                                     virtdisk_sptr_t virtdisk_w,
                                     const std::string& network_guid,
                                     const VirtualMachineDescription& desc,
                                     class VMStatusMonitor& monitor,
                                     const SSHKeyProvider& key_provider,
                                     const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir},
      description(desc),
      primary_network_guid(network_guid),
      hcs(std::move(hcs_w)),
      hcn(std::move(hcn_w)),
      monitor(monitor),
      virtdisk(std::move(virtdisk_w))
{
    // Verify that the given API wrappers are not null
    {
        const std::array<void*, 3> api_ptrs = {hcs.get(), hcn.get(), virtdisk.get()};
        if (std::any_of(std::begin(api_ptrs), std::end(api_ptrs), [](const void* ptr) {
                return nullptr == ptr;
            }))
        {
            throw InvalidAPIPointerException{"One of the required API pointers is not set: {}.",
                                             fmt::join(api_ptrs, ",")};
        }
    }

    const auto created_from_scratch = maybe_create_compute_system();
    const auto state = fetch_state_from_api();

    if (hcs->set_compute_system_callback(hcs_system,
                                         this,
                                         HCSVirtualMachine::compute_system_event_callback))
    {
        // TODO: Log
    }

    mpl::debug(kLogCategory,
               "HCSVirtualMachine::HCSVirtualMachine() > `{}`, created_from_scratch: {}, state: {}",
               vm_name,
               created_from_scratch,
               state);

    // Reflect compute system's state
    set_state(state);
    update_state();
}

void HCSVirtualMachine::compute_system_event_callback(void* event, void* context)
{
    mpl::debug(kLogCategory,
               "compute_system_event_callback() >  event: {}, context: {}",
               fmt::ptr(event),
               fmt::ptr(context));
    const auto type = hcs::parse_event(event);
    auto vm = reinterpret_cast<HCSVirtualMachine*>(context);

    switch (type)
    {
    case hcs::HcsEventType::SystemExited:
    {
        mpl::info(kLogCategory,
                  "compute_system_event_callback() > {}:  SystemExited event received",
                  vm->vm_name);
        vm->state = State::off;
        vm->update_state();
        vm->shutdown_signal.signal();
    }
    break;
    case hcs::HcsEventType::Unknown:
        mpl::info(kLogCategory,
                  "compute_system_event_callback() > {}:  Unidentified event received",
                  vm->vm_name);
        break;
    }
}

std::filesystem::path HCSVirtualMachine::get_primary_disk_path() const
{
    const std::filesystem::path base_vhdx = description.image.image_path.toStdString();
    const std::filesystem::path head_avhdx =
        base_vhdx.parent_path() / virtdisk::VirtDiskSnapshot::head_disk_name();
    return std::filesystem::exists(head_avhdx) ? head_avhdx : base_vhdx;
}

void HCSVirtualMachine::grant_access_to_paths(std::list<std::filesystem::path> paths) const
{
    // std::list, because we need iterator and pointer stability while inserting.
    // Normal for loop here because we want .end() to be evaluated in every
    // iteration since we might also insert new elements to the list.
    for (auto itr = paths.begin(); itr != paths.end(); ++itr)
    {
        const auto& path = *itr;
        mpl::debug(kLogCategory,
                   "Granting access to path `{}`, exists? {}",
                   path,
                   std::filesystem::exists(path));
        if (std::filesystem::is_symlink(path))
        {
            paths.push_back(std::filesystem::canonical(path));
        }

        if (const auto r = hcs->grant_vm_access(vm_name, path); !r)
        {
            mpl::error(kLogCategory,
                       "Could not grant access to VM `{}` for the path `{}`, error code: {}",
                       vm_name,
                       path,
                       r);
        }
    }
}

bool HCSVirtualMachine::maybe_create_compute_system()
{
    const auto result = hcs->open_compute_system(vm_name, hcs_system);
    if (result)
    {
        // Opened existing VM
        return false;
    }

    // {
    //     hcs::ComputeSystemState cs_state{hcs::ComputeSystemState::unknown};
    //     // Check if the VM already exist
    //     const auto result = hcs->get_compute_system_state(vm_name, cs_state);

    //     if (!(E_INVALIDARG == static_cast<HRESULT>(result.code)))
    //     {
    //         // Target compute system already exist, no need to re-create.
    //         return false;
    //     }
    // }

    // FIXME: Handle suspend state?

    const auto create_endpoint_params = [this]() {
        std::vector<hcn::CreateEndpointParameters> params{};

        // The primary endpoint (management)
        hcn::CreateEndpointParameters primary_endpoint{};
        primary_endpoint.network_guid = primary_network_guid;
        primary_endpoint.endpoint_guid = mac2uuid(description.default_mac_address);
        primary_endpoint.mac_address = description.default_mac_address;
        replace_colon_with_dash(primary_endpoint.mac_address.value());
        params.push_back(primary_endpoint);

        // Additional endpoints, a.k.a. extra interfaces.
        for (const auto& v : description.extra_interfaces)
        {
            hcn::CreateEndpointParameters extra_endpoint{};
            extra_endpoint.network_guid = multipass::utils::make_uuid(v.id).toStdString();
            extra_endpoint.endpoint_guid = mac2uuid(v.mac_address);
            extra_endpoint.mac_address = v.mac_address;
            replace_colon_with_dash(extra_endpoint.mac_address.value());
            params.push_back(extra_endpoint);
        }
        return params;
    }();

    for (const auto& endpoint : create_endpoint_params)
    {
        // There might be remnants from an old VM, remove the endpoint if exist before
        // creating it again.
        if (hcn->delete_endpoint(endpoint.endpoint_guid))
        {
            mpl::debug(kLogCategory,
                       "The endpoint {} was already present for the VM {}, removed it.",
                       endpoint.endpoint_guid,
                       vm_name);
        }
        if (const auto& [status, msg] = hcn->create_endpoint(endpoint); !status)
        {
            throw CreateEndpointException{"create_endpoint failed with {}", status};
        }
    }

    // E_INVALIDARG means there's no such VM.
    // Create the VM from scratch.
    const auto create_compute_system_params = [this, &create_endpoint_params]() {
        hcs::CreateComputeSystemParameters params{};
        params.name = description.vm_name;
        params.memory_size_mb = description.mem_size.in_megabytes();
        params.processor_count = description.num_cores;

        hcs::HcsScsiDevice primary_disk{hcs::HcsScsiDeviceType::VirtualDisk()};
        primary_disk.name = "Primary disk";
        primary_disk.path = get_primary_disk_path();
        primary_disk.read_only = false;
        params.scsi_devices.push_back(primary_disk);

        hcs::HcsScsiDevice cloudinit_iso{hcs::HcsScsiDeviceType::Iso()};
        cloudinit_iso.name = "cloud-init ISO file";
        cloudinit_iso.path = description.cloud_init_iso.toStdString();
        cloudinit_iso.read_only = true;
        params.scsi_devices.push_back(cloudinit_iso);

        const auto create_endpoint_to_network_adapter = [this](const auto& create_params) {
            hcs::HcsNetworkAdapter network_adapter{};
            network_adapter.endpoint_guid = create_params.endpoint_guid;
            if (!create_params.mac_address)
            {
                throw CreateEndpointException("One of the endpoints do not have a MAC address!");
            }
            network_adapter.mac_address = create_params.mac_address.value();
            return network_adapter;
        };

        std::transform(create_endpoint_params.begin(),
                       create_endpoint_params.end(),
                       std::back_inserter(params.network_adapters),
                       create_endpoint_to_network_adapter);
        return params;
    }();

    if (const auto create_result =
            hcs->create_compute_system(create_compute_system_params, hcs_system);
        !create_result)
    {
        fmt::print(L"Create compute system failed: {}", create_result.status_msg);
        throw CreateComputeSystemException{"create_compute_system failed with {}",
                                           create_result.code};
    }

    // Grant access to the VHDX and the cloud-init ISO files.
    for (const auto& scsi : create_compute_system_params.scsi_devices)
    {
        if (scsi.type == hcs::HcsScsiDeviceType::VirtualDisk())
        {
            std::vector<std::filesystem::path> lineage{};
            if (virtdisk->list_virtual_disk_chain(scsi.path.get(), lineage))
            {
                grant_access_to_paths({lineage.begin(), lineage.end()});
            }
        }
        else
        {
            grant_access_to_paths({scsi.path.get()});
        }
    }

    return true;
}

void HCSVirtualMachine::set_state(hcs::ComputeSystemState compute_system_state)
{
    mpl::debug(kLogCategory,
               "set_state() -> VM `{}` HCS state `{}`",
               vm_name,
               compute_system_state);

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

    if (state == prev_state)
        return;

    mpl::info(kLogCategory,
              "set_state() > VM {} state changed from {} to {}",
              vm_name,
              prev_state,
              state);
}

void HCSVirtualMachine::start()
{
    mpl::debug(kLogCategory, "start() -> Starting VM `{}`, current state {}", vm_name, state);

    // Create the compute system, if not created yet.
    maybe_create_compute_system();

    state = VirtualMachine::State::starting;
    update_state();
    // Resume and start are the same thing in Multipass terms
    // Try to determine whether we need to resume or start here.
    const auto& [status, status_msg] = [&] {
        // Fetch the latest state value.
        const auto hcs_state = fetch_state_from_api();
        mpl::debug(kLogCategory, "start() -> VM `{}` HCS state is `{}`", vm_name, hcs_state);
        switch (hcs_state)
        {
        case hcs::ComputeSystemState::paused:
        {
            mpl::debug(kLogCategory, "start() -> VM `{}` is in paused state, resuming", vm_name);
            return hcs->resume_compute_system(hcs_system);
        }
        case hcs::ComputeSystemState::created:
            [[fallthrough]];
        default:
        {
            mpl::debug(kLogCategory, "start() -> VM `{}` is in {} state, starting", vm_name, state);
            return hcs->start_compute_system(hcs_system);
        }
        }
    }();

    if (!status)
    {
        state = VirtualMachine::State::stopped;
        update_state();
        throw StartComputeSystemException{"Could not start the VM: {}", status};
    }

    mpl::debug(kLogCategory, "start() -> Start/resume VM `{}`, result `{}`", vm_name, status);
}
void HCSVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    mpl::debug(kLogCategory,
               "shutdown() -> Shutting down VM `{}`, current state {}",
               vm_name,
               state);

    switch (shutdown_policy)
    {
    case ShutdownPolicy::Powerdown:
        mpl::debug(kLogCategory,
                   "shutdown() -> Requested powerdown, initiating graceful shutdown for `{}`",
                   vm_name);

        // If the guest has integration modules enabled, we can use graceful shutdown.
        if (!hcs->shutdown_compute_system(hcs_system))
        {
            // Fall back to SSH shutdown.
            ssh_exec("sudo shutdown -h now");
            drop_ssh_session();
            // We need to wait here.
        }
        break;
    case ShutdownPolicy::Halt:
    case ShutdownPolicy::Poweroff:
        mpl::debug(kLogCategory,
                   "shutdown() -> Requested halt/poweroff, initiating forceful shutdown for `{}`",
                   vm_name);
        drop_ssh_session();
        // These are non-graceful variants. Just terminate the system immediately.
        hcs->terminate_compute_system(hcs_system);
        break;
    }

    // Wait until the machine shuts down.
    shutdown_signal.wait_for(std::chrono::seconds{180});
}

void HCSVirtualMachine::suspend()
{
    mpl::debug(kLogCategory, "suspend() -> Suspending VM `{}`, current state {}", vm_name, state);
    const auto& [status, status_msg] = hcs->pause_compute_system(hcs_system);
    set_state(fetch_state_from_api());
    update_state();
}

HCSVirtualMachine::State HCSVirtualMachine::current_state()
{
    return state;
}
int HCSVirtualMachine::ssh_port()
{
    return kDefaultSSHPort;
}
std::string HCSVirtualMachine::ssh_hostname(std::chrono::milliseconds /*timeout*/)
{
    return fmt::format("{}.mshome.net", vm_name);
}
std::string HCSVirtualMachine::ssh_username()
{
    return description.ssh_username;
}

std::string HCSVirtualMachine::management_ipv4()
{
    const auto& [ipv4, _] = resolve_ip_addresses(ssh_hostname({}).c_str());
    if (ipv4.empty())
    {
        mpl::error(kLogCategory, "management_ipv4() > failed to resolve `{}`", ssh_hostname({}));
        return "UNKNOWN";
    }

    const auto result = *ipv4.begin();

    mpl::trace(kLogCategory, "management_ipv4() > IP address is `{}`", result);
    // Prefer the first one
    return result;
}
std::string HCSVirtualMachine::ipv6()
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
void HCSVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] { return state != State::off; };
    multipass::backend::ensure_vm_is_running_for(this,
                                                 is_vm_running,
                                                 "Instance shutdown during start");
}
void HCSVirtualMachine::update_state()
{
    monitor.persist_state_for(vm_name, state);
}

hcs::ComputeSystemState HCSVirtualMachine::fetch_state_from_api()
{
    hcs::ComputeSystemState compute_system_state{hcs::ComputeSystemState::unknown};
    const auto result = hcs->get_compute_system_state(hcs_system, compute_system_state);
    return compute_system_state;
}

void HCSVirtualMachine::update_cpus(int num_cores)
{
    mpl::debug(kLogCategory,
               "update_cpus() -> called for VM `{}`, num_cores `{}`",
               vm_name,
               num_cores);
    // This is a no-op since HCS creates the VM from scratch
    // every time in a cold boot.
}

void HCSVirtualMachine::resize_memory(const MemorySize& new_size)
{
    mpl::debug(kLogCategory,
               "resize_memory() -> called for VM `{}`, new_size `{}` MiB",
               vm_name,
               new_size.in_megabytes());
    hcs::HcsRequest req{
        hcs::HcsResourcePath::Memory(),
        hcs::HcsRequestType::Update(),
        hcs::HcsModifyMemorySettings{static_cast<std::uint32_t>(new_size.in_megabytes())}};
    hcs->modify_compute_system(hcs_system, req);
    // FIXME: Log the result.
}

void HCSVirtualMachine::resize_disk(const MemorySize& new_size)
{
    mpl::debug(kLogCategory,
               "resize_disk() -> called for VM `{}`, new_size `{}` MiB",
               vm_name,
               new_size.in_megabytes());
    throw std::runtime_error{"Not yet implemented"};
}

void HCSVirtualMachine::add_network_interface(int index,
                                              const std::string& default_mac_addr,
                                              const NetworkInterface& extra_interface)
{
    mpl::debug(kLogCategory,
               "add_network_interface() -> called for VM `{}`, index: {}, default_mac: {}, "
               "extra_interface: (mac: {}, "
               "mac_address: {}, id: {})",
               vm_name,
               index,
               default_mac_addr,
               extra_interface.mac_address,
               extra_interface.auto_mode,
               extra_interface.id);
    add_extra_interface_to_instance_cloud_init(default_mac_addr, extra_interface);
    if (!(state == VirtualMachine::State::stopped))
    {
        // No need to do it for stopped machines
        mpl::info(kLogCategory,
                  "add_network_interface() -> Skipping hot-plug, VM is in a stopped state.");
        return;
    }

    // Hot-plug a network card.
    const auto endpoint = [&extra_interface]() {
        hcn::CreateEndpointParameters endpoint{};
        endpoint.network_guid = multipass::utils::make_uuid(extra_interface.id).toStdString();
        endpoint.endpoint_guid = mac2uuid(extra_interface.mac_address);
        endpoint.mac_address = extra_interface.mac_address;
        replace_colon_with_dash(endpoint.mac_address.value());
        return endpoint;
    }();

    if (const auto result = hcn->create_endpoint(endpoint); !result)
    {
        mpl::error(kLogCategory,
                   "add_network_interface() -> failed to create endpoint for `{}`",
                   extra_interface.id);
        return;
    }

    const auto add_network_adapter_req = [&endpoint]() {
        hyperv::hcs::HcsNetworkAdapter network_adapter{};
        network_adapter.endpoint_guid = endpoint.endpoint_guid;
        network_adapter.mac_address = endpoint.mac_address.value();

        hcs::HcsRequest add_network_adapter_req{
            hcs::HcsResourcePath::NetworkAdapters(network_adapter.endpoint_guid),
            hcs::HcsRequestType::Add(),
            network_adapter};
        return add_network_adapter_req;
    }();

    if (const auto result = hcs->modify_compute_system(hcs_system, add_network_adapter_req);
        !result)
    {
        mpl::error(kLogCategory,
                   "add_network_interface() -> failed to add endpoint for network `{}` to compute "
                   "system `{}`",
                   extra_interface.id,
                   vm_name);
        return;
    }
}
std::unique_ptr<MountHandler>
HCSVirtualMachine::make_native_mount_handler(const std::string& target, const VMMount& mount)
{
    mpl::debug(kLogCategory,
               "make_native_mount_handler() -> called for VM `{}`, target: {}",
               vm_name,
               target);
    // throw NotImplementedOnThisBackendException{
    //     "Plan9 mounts require an agent running on guest, which is not implemented yet."};
    // FIXME: Replace with Plan9 mount handler once the guest agent is available.

    static const SmbManager smb_manager{};
    return std::make_unique<SmbMountHandler>(this,
                                             &key_provider,
                                             target,
                                             mount,
                                             instance_dir.absolutePath(),
                                             smb_manager);
}

std::shared_ptr<Snapshot> HCSVirtualMachine::make_specific_snapshot(
    const std::string& snapshot_name,
    const std::string& comment,
    const std::string& instance_id,
    const VMSpecs& specs,
    std::shared_ptr<Snapshot> parent)
{
    return std::make_shared<virtdisk::VirtDiskSnapshot>(snapshot_name,
                                                        comment,
                                                        instance_id,
                                                        parent,
                                                        specs,
                                                        *this,
                                                        description,
                                                        virtdisk);
}

std::shared_ptr<Snapshot> HCSVirtualMachine::make_specific_snapshot(const QString& filename)
{
    return std::make_shared<virtdisk::VirtDiskSnapshot>(filename, *this, description, virtdisk);
}

} // namespace multipass::hyperv
