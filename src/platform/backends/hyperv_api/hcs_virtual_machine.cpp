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
#include <hyperv_api/hcn/hyperv_hcn_endpoint_naming.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper.h>
#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_event_type.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper.h>
#include <hyperv_api/hcs_virtual_machine_exceptions.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#include <hyperv_api/virtdisk/virtdisk_snapshot.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <shared/windows/smb_mount_handler.h>

#include <multipass/constants.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/top_catch_all.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <fmt/xchar.h>
#include <scope_guard.hpp>

#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace
{

namespace mp = multipass;
namespace mpl = mp::logging;
using mp::hyperv::hcn::HCN;
using mp::hyperv::hcs::HCS;
using mp::hyperv::virtdisk::VirtDisk;
using namespace mp::hyperv;

inline auto replace_colon_with_dash(const std::string& addr)
{
    if (addr.empty())
        return addr;
    std::string result{addr};
    std::ranges::replace(result, ':', '-');
    return result;
}

void try_create_endpoints(
    const std::string& vm_name,
    const std::vector<multipass::hyperv::hcn::CreateEndpointParameters>& create_endpoint_params)
{
    std::vector<multipass::hyperv::hcn::CreateEndpointParameters> created_endpoints;
    for (const auto& endpoint : create_endpoint_params)
    {
        if (HCN().delete_endpoint(endpoint.endpoint_guid))
        {
            mpl::warn(vm_name,
                      "Endpoint {} was already present, removed it.",
                      endpoint.endpoint_guid);
        }
        if (const auto result = HCN().create_endpoint(endpoint))
            created_endpoints.push_back(endpoint);
        else
        {
            for (const auto& created_ep : created_endpoints)
            {
                mpl::warn(vm_name,
                          "Removing endpoint {} due to failed operation, result {}",
                          created_ep.endpoint_guid,
                          HCN().delete_endpoint(created_ep.endpoint_guid));
            }
            throw multipass::hyperv::CreateEndpointException{
                "create_endpoint failed with {}, endpoint details: {}",
                result.code,
                endpoint};
        }
    }
}

} // namespace

namespace multipass::hyperv
{

HCSVirtualMachine::HCSVirtualMachine(const std::string& network_guid,
                                     const VirtualMachineDescription& desc,
                                     class VMStatusMonitor& monitor,
                                     const SSHKeyProvider& key_provider,
                                     AvailabilityZone& zone,
                                     const Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, desc, key_provider, zone, instance_dir},
      description(desc),
      primary_network_guid(network_guid),
      monitor(monitor)
{
    const auto created_from_scratch = maybe_create_compute_system();
    const auto compute_state = fetch_state_from_api();

    mpl::debug(get_name(),
               "HCSVirtualMachine() > created_from_scratch: {}, state: {}",
               created_from_scratch,
               compute_state);

    // Reflect compute system's state
    const auto prev_state = state;
    set_state(compute_state);

    // Persist initial state even if unchanged
    if (prev_state == this->state)
        HCSVirtualMachine::handle_state_update();
}

HCSVirtualMachine::~HCSVirtualMachine()
{
    top_catch_all(vm_name, [this]() {
        if (current_state() != State::running)
            return;

        // Auto-suspend if running
        suspend();
        // Persist previous VM state
        set_state(State::running);
    });
}

void HCSVirtualMachine::compute_system_event_callback(HCS_EVENT* event, void* context)
{
    const auto type = hcs::parse_event(event);
    auto vm = static_cast<HCSVirtualMachine*>(context);

    mpl::debug(vm->get_name(),
               "compute_system_event_callback() >  event: {}, context: {}",
               fmt::ptr(event),
               fmt::ptr(context));

    switch (type)
    {
    case hcs::HcsEventType::SystemExited:
    {
        mpl::info(vm->get_name(), "compute_system_event_callback() > SystemExited event received");
        vm->set_state(State::off);
        vm->termination_signal.signal();
    }
    break;
    case hcs::HcsEventType::Unknown:
    default:
        mpl::warn(vm->get_name(), "compute_system_event_callback() > Unidentified event received");
        break;
    }
}

std::filesystem::path HCSVirtualMachine::get_guest_state_file_path() const
{
    return std::filesystem::path{description.image.image_path}.replace_extension(".vmgs");
}
std::filesystem::path HCSVirtualMachine::get_runtime_state_file_path() const
{
    return std::filesystem::path{description.image.image_path}.replace_extension(".vmrs");
}

std::filesystem::path HCSVirtualMachine::get_saved_state_file_path() const
{
    return std::filesystem::path{description.image.image_path}.replace_extension(
        ".SavedState.vmrs");
}

bool HCSVirtualMachine::has_saved_state_file() const
{
    return MP_FILEOPS.exists(get_saved_state_file_path());
}

std::filesystem::path HCSVirtualMachine::get_primary_disk_path() const
{
    return description.image.image_path;
}

void HCSVirtualMachine::grant_access_to_scsi_device(const hcs::HcsScsiDevice& device) const
{
    if (device.type == hcs::HcsScsiDeviceType::VirtualDisk())
    {
        std::vector<std::filesystem::path> lineage{};
        if (VirtDisk().list_virtual_disk_chain(device.path.get(), lineage))
        {
            grant_access_to_paths({lineage.begin(), lineage.end()});
        }
    }
    else
    {
        grant_access_to_paths({device.path.get()});
    }
}

void HCSVirtualMachine::grant_access_to_paths(std::list<std::filesystem::path> paths) const
{
    // std::list, because we need iterator and pointer stability while inserting.
    // Normal for loop here because we want .end() to be evaluated in every
    // iteration since we might also insert new elements to the list.
    for (auto itr = paths.begin(); itr != paths.end(); ++itr)
    {
        const auto& path = *itr;
        mpl::debug(get_name(),
                   "Granting access to path `{}`, exists? {}",
                   path,
                   MP_FILEOPS.exists(path));
        if (MP_FILEOPS.is_symlink(path))
        {
            paths.push_back(std::filesystem::canonical(path));
        }

        if (const auto r = HCS().grant_vm_access(get_name(), path); !r)
        {
            mpl::error(get_name(),
                       "Could not grant access for the path `{}`, error code: {}",
                       path,
                       r);
        }
    }
}

void HCSVirtualMachine::set_compute_system_callback_handler()
{
    if (hcs_system)
    {
        top_catch_all(get_name(), [this] {
            if (!HCS().set_compute_system_callback(
                    hcs_system,
                    this,
                    HCSVirtualMachine::compute_system_event_callback))
            {
                mpl::warn(get_name(), "Could not set compute system callback!");
            }
        });
    }
}

std::vector<hcn::CreateEndpointParameters> HCSVirtualMachine::make_endpoint_parameters() const
{
    // Deterministic, instance-based name tagged onto every endpoint that belongs to this
    // VM. It allows the endpoints to be discovered and removed by name later on (e.g. during
    // instance purge), without needing to reopen the compute system to retrieve its RuntimeId.
    const auto endpoint_name = hcn::endpoint_name_for(description.vm_name);

    std::vector<hcn::CreateEndpointParameters> params{
        // The primary endpoint (management)
        {.network_guid = primary_network_guid,
         .endpoint_guid = endpoint_guid_for_mac(description.default_mac_address),
         .mac_address = replace_colon_with_dash(description.default_mac_address),
         .name = endpoint_name}};

    // Additional endpoints, a.k.a. extra interfaces. Their networks are referred to by name, and
    // are looked up since only networks Multipass created have GUIDs derived from their names.
    for (const auto& extra : description.extra_interfaces)
    {
        const auto network_guid = network_guid_for_name(extra.id);
        if (!network_guid)
            throw CreateEndpointException{"Could not find network `{}` for interface {}",
                                          extra.id,
                                          extra.mac_address};

        params.push_back({.network_guid = *network_guid,
                          .endpoint_guid = endpoint_guid_for_mac(extra.mac_address),
                          .mac_address = replace_colon_with_dash(extra.mac_address),
                          .name = endpoint_name});
    }

    return params;
};

bool HCSVirtualMachine::maybe_open_compute_system()
{
    hcs_system.reset();

    if (const auto result = HCS().open_compute_system(get_name(), hcs_system))
    {
        set_compute_system_callback_handler();
        return true;
    }
    else if (HCS_E_SYSTEM_NOT_FOUND != static_cast<HRESULT>(result.code))
    {
        throw OpenComputeSystemException{"Failed with error code: {}", result.code};
    }

    return false;
}

bool HCSVirtualMachine::maybe_create_compute_system()
{
    if (maybe_open_compute_system())
        return false;

    // Create the VM from scratch.
    if (!has_saved_state_file() &&
        !remove_management_ipv4_neighbors(primary_network_guid, description.default_mac_address))
        mpl::warn(get_name(), "Could not remove all stale management IP entries");

    const auto endpoints = make_endpoint_parameters();

    try_create_endpoints(get_name(), endpoints);
    auto rollback_creation = sg::make_scope_guard([&]() noexcept {
        if (hcs_system)
            (void)HCS().terminate_compute_system(hcs_system);
        // Drop the dead handle so that the next state query reopens the compute system.
        hcs_system.reset();

        for (const auto& endpoint : endpoints)
            (void)HCN().delete_endpoint(endpoint.endpoint_guid);
    });

    const hcs::CreateComputeSystemParameters create_compute_system_params{
        .name = description.vm_name,
        .memory_size_mb = static_cast<uint32_t>(description.mem_size.in_megabytes()),
        .processor_count = static_cast<uint32_t>(description.num_cores),
        .scsi_devices = {{.type = hcs::HcsScsiDeviceType::VirtualDisk(),
                          .name = "Primary disk",
                          .path = get_primary_disk_path(),
                          .read_only = false},
                         {.type = hcs::HcsScsiDeviceType::Iso(),
                          .name = "cloud-init ISO file",
                          .path = description.cloud_init_iso.toStdString(),
                          .read_only = true}},
        .network_adapters =
            [&] {
                const auto view = endpoints |
                                  std::views::transform(
                                      [](const auto& endpoint) -> hcs::HcsNetworkAdapter {
                                          return {.endpoint_guid = endpoint.endpoint_guid,
                                                  .mac_address = endpoint.mac_address.value()};
                                      });
                return std::vector(std::ranges::begin(view), std::ranges::end(view));
            }(),
        .guest_state = {.guest_state_file_path = get_guest_state_file_path(),
                        .runtime_state_file_path = get_runtime_state_file_path(),
                        .save_state_file_path = has_saved_state_file()
                                                  ? std::optional(get_saved_state_file_path())
                                                  : std::nullopt}};

    if (const auto create_result = HCS().create_compute_system(create_compute_system_params,
                                                               hcs_system);
        !create_result)
    {
        throw CreateComputeSystemException{"create_compute_system failed with {}",
                                           create_result.code};
    }

    // Grant access to the VHDX and the cloud-init ISO files.
    for (const auto& scsi : create_compute_system_params.scsi_devices)
    {
        grant_access_to_scsi_device(scsi);
    }

    // Also grant access to the VM folder itself
    grant_access_to_paths({instance_dir.absolutePath().toStdString()});
    rollback_creation.dismiss();
    set_compute_system_callback_handler();
    return true;
}

void HCSVirtualMachine::set_state(hcs::ComputeSystemState compute_system_state)
{
    mpl::debug(get_name(), "set_state() -> HCS state `{}`", compute_system_state);

    if (state == State::unavailable)
    {
        mpl::debug(get_name(), "set_state() -> Zone is unavailable");
        return;
    }

    switch (compute_system_state)
    {
    case hcs::ComputeSystemState::created:
        set_state(State::off);
        break;
    case hcs::ComputeSystemState::paused:
        mpl::debug(vm_name, "VM is paused but not completely suspended");
        set_state(State::suspended);
        break;
    case hcs::ComputeSystemState::running:
        set_state(State::running);
        break;
    case hcs::ComputeSystemState::saved_as_template:
    case hcs::ComputeSystemState::stopped:
        set_state(has_saved_state_file() ? State::suspended : State::stopped);
        break;
    case hcs::ComputeSystemState::unknown:
        set_state(State::unknown);
        break;
    }
}

void HCSVirtualMachine::set_state(VirtualMachine::State new_state)
{
    if (state == new_state)
        return;

    mpl::info(get_name(), "set_state() -> State changed from {} to {}", state, new_state);
    state = new_state;
    handle_state_update();
}

void HCSVirtualMachine::start()
{
    mpl::debug(get_name(), "start() -> Starting VM, current state {}", state);

    // Create the compute system, if not created yet.
    const auto created_from_scratch = maybe_create_compute_system();
    if (created_from_scratch)
        mpl::debug(get_name(), "start() -> VM was not present, created from scratch");

    const auto hcs_state = fetch_state_from_api();
    const auto is_cold_start = hcs_state == hcs::ComputeSystemState::created ||
                               hcs_state == hcs::ComputeSystemState::stopped;
    if (!created_from_scratch && is_cold_start && !has_saved_state_file() &&
        !remove_management_ipv4_neighbors(primary_network_guid, description.default_mac_address))
    {
        mpl::warn(get_name(), "Could not remove all stale management IP entries");
    }

    const auto prev_state = state;
    set_state(VirtualMachine::State::starting);
    // Resume and start are the same thing in Multipass terms
    // Try to determine whether we need to resume or start here.
    const auto result = [&] {
        switch (hcs_state)
        {
        case hcs::ComputeSystemState::paused:
        {
            mpl::debug(get_name(), "start() -> VM is in paused state, resuming");
            return HCS().resume_compute_system(hcs_system);
        }
        case hcs::ComputeSystemState::created:
            [[fallthrough]];
        default:
        {
            mpl::debug(get_name(), "start() -> VM is in {} state, starting", state);
            return HCS().start_compute_system(hcs_system);
        }
        }
    }();

    if (!result)
    {
        set_state(prev_state);
        throw StartComputeSystemException{"Could not start the VM: {}", result};
    }
    else if (has_saved_state_file())
    {
        mpl::trace(get_name(), "start() -> Saved state file exits, attempting to remove");
        std::error_code ec{};
        if (!MP_FILEOPS.remove(get_saved_state_file_path(), ec))
        {
            mpl::warn(get_name(),
                      "start() -> Could not remove the saved state file, error: {}",
                      ec);
        }
    }

    mpl::debug(get_name(), "start() -> result `{}`", result);
}

void HCSVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    mpl::debug(get_name(), "shutdown() -> Shutting down, current state {}", state);
    termination_signal.reset();
    try
    {
        check_state_for_shutdown(shutdown_policy);
    }
    catch (const VMStateIdempotentException& e)
    {
        mpl::info(vm_name, "{}", e.what());
        return;
    }

    // Ensure that the ssh session is dropped at the end of this function, even if shutdown
    // fails, since it means that the VM is in some sort of error state.
    auto drop_ssh_session_sg = sg::make_scope_guard([this]() noexcept { drop_ssh_session(); });

    switch (shutdown_policy)
    {
    case ShutdownPolicy::Powerdown:
        mpl::debug(get_name(), "shutdown() -> Requested powerdown, initiating graceful shutdown");

        // If the guest has integration modules enabled, we can use graceful shutdown.
        if (!HCS().shutdown_compute_system(hcs_system))
        {
            // Fall back to SSH shutdown.
            ssh_exec("sudo shutdown -h now");
        }
        break;
    case ShutdownPolicy::Halt:
    case ShutdownPolicy::Poweroff:
        mpl::debug(get_name(),
                   "shutdown() -> Requested halt/poweroff, initiating forceful shutdown");

        // FIXME: There is a rare case where suspend fails, and fails to terminate
        // the VM as well. In this case the VM will be "paused". This is not handled
        // for now.
        if (state == State::suspended)
        {
            if (const auto ec = remove_saved_state_file_if_exists(); ec)
                throw ShutdownComputeSystemException("Could not remove state file '{}': {}",
                                                     get_saved_state_file_path(),
                                                     ec);
            update_current_state();
            return;
        }
        // These are non-graceful variants. Just terminate the system immediately.
        const auto r = HCS().terminate_compute_system(hcs_system);
        if (!r)
            throw ShutdownComputeSystemException("Could not terminate VM `{}`: {}", get_name(), r);
        mpl::debug(get_name(), "shutdown -> terminate_compute_system result: {}", r.code);
        break;
    }

    // We need to wait here.
    if (!termination_signal.wait_for(vm_shutdown_timeout))
        throw ShutdownComputeSystemException("timed out waiting for VM shutdown to complete");

    switch (auto s = current_state())
    {
    case VirtualMachine::State::stopped:
    case VirtualMachine::State::off:
    case VirtualMachine::State::suspended:
        break;
    default:
        throw ShutdownComputeSystemException("VM is not in stopped state after termination: {}", s);
        break;
    }
}

void HCSVirtualMachine::suspend()
{
    mpl::debug(get_name(), "suspend() -> Suspending, current state {}", state);

    if (const auto pause_result = HCS().pause_compute_system(hcs_system); !pause_result)
        throw SaveComputeSystemException{"Could not pause VM for suspend: {}", pause_result};

    if (const auto save_result = HCS().save_compute_system(hcs_system, get_saved_state_file_path());
        !save_result)
    {
        recover_from_failed_save();
        throw SaveComputeSystemException{"Failed to save suspended VM state to disk: {}",
                                         save_result};
    }

    // NOTE: We intentionally keep the old state here because updating it would map "paused" to
    // "suspended", causing shutdown to remove the newly saved state file.
    shutdown(ShutdownPolicy::Poweroff);
    return;
}

HCSVirtualMachine::State HCSVirtualMachine::current_state()
try
{
    if (!hcs_system && !maybe_open_compute_system())
    {
        if (state != State::unavailable)
            state = has_saved_state_file() ? State::suspended : State::off;

        return state;
    }

    update_current_state();
    return state;
}
catch (const OpenComputeSystemException& e)
{
    // Callers such as daemon startup and `list` do not expect state queries to throw.
    mpl::warn(get_name(), "current_state() > {}", e.what());
    set_state(hcs::ComputeSystemState::unknown);
    return state;
}

void HCSVirtualMachine::update_current_state()
{
    set_state(fetch_state_from_api());
}

int HCSVirtualMachine::ssh_port()
{
    return default_ssh_port;
}
std::string HCSVirtualMachine::ssh_hostname()
{
    return require_management_ipv4().as_string();
}
std::string HCSVirtualMachine::ssh_username()
{
    return description.ssh_username;
}

std::optional<IPAddress> HCSVirtualMachine::management_ipv4()
{
    const auto endpoint_guid = endpoint_guid_for_mac(description.default_mac_address);
    hcn::HcnEndpointInfo endpoint_info;
    if (const auto query_result = HCN().query_endpoint(endpoint_guid, endpoint_info); !query_result)
    {
        mpl::error(get_name(),
                   "management_ipv4() > failed to query endpoint `{}`: {}",
                   endpoint_guid,
                   query_result);
        return std::nullopt;
    }

    auto make_ip_address = [this](const std::string& addr_str) {
        IPAddress address{addr_str};
        mpl::trace(get_name(), "management_ipv4() > IP address is `{}`", address.as_string());
        return address;
    };

    for (const auto& ip_address : endpoint_info.ip_addresses)
    {
        try
        {
            return make_ip_address(ip_address);
        }
        catch (const std::invalid_argument&)
        {
            // HCN also reports IPv6 configurations, which IPAddress does not represent.
        }
    }

    if (endpoint_info.mac_address)
    {
        if (const auto ip_address = management_ipv4_neighbor(primary_network_guid,
                                                             *endpoint_info.mac_address))
        {
            return make_ip_address(*ip_address);
        }
    }

    return std::nullopt;
}

void HCSVirtualMachine::restore_snapshot(const std::string& name, VMSpecs& specs)
{
    // Restoring replaces the active VHD chain, so discard the system configured for the old chain.
    const auto resources_released = release_hcs_resources(get_name());
    hcs_system.reset();
    if (!resources_released)
    {
        throw ComputeSystemStateException{
            "Could not release HCS resources before restoring snapshot '{}.{}'",
            get_name(),
            name};
    }

    BaseVirtualMachine::restore_snapshot(name, specs);
}

void HCSVirtualMachine::handle_state_update()
{
    monitor.persist_state_for(get_name(), state);
}

hcs::ComputeSystemState HCSVirtualMachine::fetch_state_from_api() const
{
    hcs::ComputeSystemState compute_system_state{hcs::ComputeSystemState::unknown};
    const auto r = HCS().get_compute_system_state(hcs_system, compute_system_state);
    return compute_system_state;
}

void HCSVirtualMachine::update_cpus(int num_cores)
{
    mpl::debug(get_name(), "update_cpus() -> num_cores `{}`", num_cores);
    description.num_cores = num_cores;
}

void HCSVirtualMachine::resize_memory(const MemorySize& new_size)
{
    mpl::debug(get_name(), "resize_memory() -> new_size `{}` MiB", new_size.in_megabytes());
    description.mem_size = new_size;
}

void HCSVirtualMachine::resize_disk_impl(const MemorySize& new_size)
{
    mpl::debug(get_name(), "resize_disk() -> new_size `{}` MiB", new_size.in_megabytes());

    if (get_num_snapshots() > 0)
    {
        throw ResizeDiskException{"Cannot resize the primary disk while there are "
                                  "snapshots. To resize, delete the snapshots first."};
    }

    if (const auto result = VirtDisk().resize_virtual_disk(description.image.image_path,
                                                           new_size.in_bytes());
        !result)
    {
        throw ResizeDiskException{"Disk resize failed, details: {}", result};
    }
    description.disk_space = new_size;
}

void HCSVirtualMachine::add_network_interface(int index,
                                              const std::string& default_mac_addr,
                                              const NetworkInterface& extra_interface)
{
    mpl::debug(get_name(),
               "add_network_interface() -> index: {}, default_mac: {}, "
               "extra_interface: (mac: {}, "
               "mac_address: {}, id: {})",
               index,
               default_mac_addr,
               extra_interface.mac_address,
               extra_interface.auto_mode,
               extra_interface.id);
    add_extra_interface_to_instance_cloud_init(default_mac_addr, extra_interface);
    if (!(state == VirtualMachine::State::stopped))
    {
        // No need to do it for stopped machines
        mpl::info(get_name(),
                  "add_network_interface() -> Skipping hot-plug, VM is in a non-stopped state.");
        return;
    }
}
std::unique_ptr<MountHandler>
HCSVirtualMachine::make_native_mount_handler(const std::string& target, const VMMount& mount)
{
    mpl::debug(get_name(), "make_native_mount_handler() -> target: {}", target);

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
                                                        description);
}

std::shared_ptr<Snapshot> HCSVirtualMachine::make_specific_snapshot(const QString& filename)
{
    return std::make_shared<virtdisk::VirtDiskSnapshot>(filename.toStdWString(),
                                                        *this,
                                                        description);
}

std::error_code HCSVirtualMachine::remove_saved_state_file_if_exists()
{
    if (has_saved_state_file())
    {
        mpl::trace(get_name(), "Saved state file exists, attempting to remove");
        std::error_code ec{};
        if (!MP_FILEOPS.remove(get_saved_state_file_path(), ec))
        {
            // FIXME: If the VM is stopped or terminated, it will still be reported as
            // suspended because the saved-state file exists.
            mpl::warn(get_name(), "Could not remove the saved state file, error: {}", ec);
            return ec;
        }
    }

    return {};
}

void HCSVirtualMachine::recover_from_failed_save()
{
    remove_saved_state_file_if_exists();

    const auto resume_result = HCS().resume_compute_system(hcs_system);
    if (resume_result)
    {
        update_current_state();
        return;
    }

    mpl::error(get_name(),
               "Could not resume after failed suspend ({}); powering off",
               resume_result);
    try
    {
        shutdown(ShutdownPolicy::Poweroff);
    }
    catch (const std::exception& e)
    {
        mpl::error(get_name(), "Power off after failed suspend also failed: {}", e.what());
    }
}

} // namespace multipass::hyperv
