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

#include "hyperv_virtual_machine.h"
#include "hyperv_snapshot.h"

#include <multipass/constants.h>
#include <multipass/exceptions/not_implemented_on_this_backend_exception.h> // TODO@snapshots drop
#include <multipass/exceptions/start_exception.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>
#include <multipass/vm_status_monitor.h>
#include <shared/shared_backend_utils.h>
#include <shared/windows/powershell.h>
#include <shared/windows/smb_mount_handler.h>

#include <fmt/format.h>

#include <winsock2.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const QString default_switch_guid{"C08CB7B8-9B3C-408E-8E30-5E16A3AEB444"};
const QString snapshot_name{"suspend"};

namespace fs = std::filesystem;

QString quoted(const QString& str)
{
    return '"' + str + '"';
}

std::optional<mp::IPAddress> remote_ip(const std::string& host,
                                       int port,
                                       const std::string& username,
                                       const mp::SSHKeyProvider& key_provider)
try
{
    mp::SSHSession session{host, port, username, key_provider};

    sockaddr_in addr{};
    int size = sizeof(addr);
    auto socket = ssh_get_fd(session);
    const auto failed = getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &size);

    if (failed)
        return std::nullopt;

    return std::optional<mp::IPAddress>{ntohl(addr.sin_addr.s_addr)};
}
catch (...)
{
    return std::nullopt;
}

auto instance_state_for(mp::PowerShell* power_shell, const QString& name)
{
    QString state;

    if (power_shell->run(
            {"Get-VM", "-Name", name, "|", "Select-Object", "-ExpandProperty", "State"},
            &state,
            nullptr,
            /* whisper = */ true)) // avoid GUI polling spamming the logs
    {
        if (state == "Running")
        {
            return mp::VirtualMachine::State::running;
        }
        else if (state == "Starting")
        {
            return mp::VirtualMachine::State::starting;
        }
        else if (state == "Saved")
        {
            return mp::VirtualMachine::State::suspended;
        }
        else if (state == "Off")
        {
            return mp::VirtualMachine::State::stopped;
        }
    }

    return mp::VirtualMachine::State::unknown;
}

void add_extra_net(mp::PowerShell& ps,
                   const QString& vm_name,
                   const mp::NetworkInterface& extra_interface)
{
    const auto switch_name = quoted(QString::fromStdString(extra_interface.id));
    ps.easy_run({"Get-VMSwitch", "-Name", switch_name},
                fmt::format("Could not find the device to connect to: no switch named \"{}\"",
                            extra_interface.id));
    ps.easy_run({"Add-VMNetworkAdapter",
                 "-VMName",
                 vm_name,
                 "-SwitchName",
                 switch_name,
                 "-StaticMacAddress",
                 quoted(QString::fromStdString(extra_interface.mac_address))},
                fmt::format("Could not setup adapter for {}", extra_interface.id));
}

fs::path locate_vmcx_file(const fs::path& exported_vm_dir_path)
{
    if (fs::exists(exported_vm_dir_path) && fs::is_directory(exported_vm_dir_path))
    {
        const fs::path vm_state_dir = exported_vm_dir_path / "Virtual Machines";

        if (auto iter = std::find_if(fs::directory_iterator{vm_state_dir},
                                     fs::directory_iterator{},
                                     [](const fs::directory_entry& entry) -> bool {
                                         return entry.path().extension() == ".vmcx";
                                     });
            iter != fs::directory_iterator{})
        {
            return iter->path();
        }
    }

    return {};
}
} // namespace

mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc,
                                               VMStatusMonitor& monitor,
                                               const SSHKeyProvider& key_provider,
                                               const mp::Path& instance_dir)
    : HyperVVirtualMachine{desc, monitor, key_provider, instance_dir, true}
{
    if (!power_shell->run({"Get-VM", "-Name", name}))
    {
        auto mem_size =
            QString::number(desc.mem_size.in_bytes()); /* format documented in `Help(New-VM)`, under
`-MemoryStartupBytes` option; */

        power_shell->easy_run({QString("$switch = Get-VMSwitch -Id %1").arg(default_switch_guid)},
                              "Could not find the default switch");

        power_shell->easy_run({"New-VM",
                               "-Name",
                               name,
                               "-Generation",
                               "2",
                               "-VHDPath",
                               '"' + desc.image.image_path + '"',
                               "-BootDevice",
                               "VHD",
                               "-SwitchName",
                               "$switch.Name",
                               "-MemoryStartupBytes",
                               mem_size},
                              "Could not create VM");
        power_shell->easy_run({"Set-VMFirmware", "-VMName", name, "-EnableSecureBoot", "Off"},
                              "Could not disable secure boot");
        power_shell->easy_run(
            {"Set-VMProcessor", "-VMName", name, "-Count", QString::number(desc.num_cores)},
            "Could not configure VM processor");
        power_shell->easy_run(
            {"Add-VMDvdDrive", "-VMName", name, "-Path", '"' + desc.cloud_init_iso + '"'},
            "Could not setup cloud-init drive");
        power_shell->easy_run({"Set-VMMemory", "-VMName", name, "-DynamicMemoryEnabled", "$false"},
                              "Could not disable dynamic memory");
        power_shell->easy_run({"Set-VM", "-Name", name, "-AutomaticCheckpointsEnabled", "$false"},
                              "Could not disable automatic snapshots");

        setup_network_interfaces();

        state = State::off;
    }
    else
    {
        state = instance_state_for(power_shell.get(), name);
    }
}

mp::HyperVVirtualMachine::HyperVVirtualMachine(const std::string& source_vm_name,
                                               const VMSpecs& src_vm_specs,
                                               const VirtualMachineDescription& desc,
                                               VMStatusMonitor& monitor,
                                               const SSHKeyProvider& key_provider,
                                               const Path& dest_instance_dir)
    : HyperVVirtualMachine{desc, monitor, key_provider, dest_instance_dir, true}
{
    // 1. Export-VM -Name vm1 -Path C:\ProgramData\Multipass\data\vault\instances\vm1-clone1
    power_shell->easy_run({"Export-VM",
                           "-Name",
                           QString::fromStdString(source_vm_name),
                           "-Path",
                           quoted(dest_instance_dir)},
                          "Could not export the source vm");

    // 2. $imported_vm=Import-VM -Path
    // 'C:\ProgramData\Multipass\data\vault\instances\vm1-clone1\vm1\Virtual
    // Machines\7735327A-A22F-4926-95A1-51757D650BB7.vmcx' -Copy -GenerateNewId -VhdDestinationPath
    // "C:\ProgramData\Multipass\data\vault\instances\vm1-clone1\"
    const fs::path exported_vm_path =
        fs::path{dest_instance_dir.toStdString()} / fs::path{source_vm_name};
    const fs::path vmcx_file_path = locate_vmcx_file(exported_vm_path);
    // The next step needs to rename the instance, so we need to store the instance variable
    // $imported_vm from the Import-VM step. Because we can not use vm name to uniquely identify the
    // vm due to the imported vm has the same name.
    power_shell->easy_run({"$imported_vm=Import-VM",
                           "-Path",
                           quoted(QString::fromStdString(vmcx_file_path.string())),
                           "-Copy",
                           "-GenerateNewId",
                           "-VhdDestinationPath",
                           quoted(dest_instance_dir)},
                          "Could not import from the exported instance directory");
    // 3. Rename-vm $imported_vm -NewName vm1-clone1
    power_shell->easy_run({"Rename-vm", "$imported_vm", "-NewName", name},
                          "Could not rename the imported vm");
    // 4. Remove-VMDvdDrive -VMName vm1-clone1 -ControllerNumber 0 -ControllerLocation 1
    power_shell->easy_run(
        {"Remove-VMDvdDrive",
         "-VMName",
         name,
         "-ControllerNumber",
         QString::number(0),
         "-ControllerLocation",
         QString::number(1)},
        "Could not remove the cloud-init-config.iso file from the virtual machine");
    // 5. Add-VMDvdDrive -VMName vm1-clone1 -Path
    // 'C:\ProgramData\Multipass\data\vault\instances\vm1-clone1\cloud-init-config.iso'
    const fs::path dest_cloud_init_path =
        fs::path{dest_instance_dir.toStdString()} / cloud_init_file_name;
    power_shell->easy_run({"Add-VMDvdDrive",
                           "-VMName",
                           name,
                           "-Path",
                           quoted(QString::fromStdString(dest_cloud_init_path.string()))},
                          "Could not add the cloud-init-config.iso to the virtual machine");
    // 6. Reset the default address, and extra interface addresses
    update_network_interfaces(src_vm_specs);

    state = State::off;

    remove_snapshots_from_backend();
    fs::remove_all(exported_vm_path);
}

mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc,
                                               VMStatusMonitor& monitor,
                                               const SSHKeyProvider& key_provider,
                                               const Path& instance_dir,
                                               bool /*is_internal*/)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir},
      desc{desc},
      name{QString::fromStdString(desc.vm_name)},
      power_shell{std::make_unique<PowerShell>(vm_name)},
      monitor{&monitor}
{
}

void mp::HyperVVirtualMachine::setup_network_interfaces()
{
    power_shell->easy_run({"Set-VMNetworkAdapter",
                           "-VMName",
                           name,
                           "-StaticMacAddress",
                           quoted(QString::fromStdString(desc.default_mac_address))},
                          "Could not setup default adapter");

    for (const auto& net : desc.extra_interfaces)
    {
        add_extra_net(*power_shell, name, net);
    }
}

void mp::HyperVVirtualMachine::update_network_interfaces(const VMSpecs& src_specs)
{
    // We use mac address to identify the corresponding network adapter, it is a cumbersome
    // implementation because the update requires the original default mac address and extra
    // interface mac addresses. Meanwhile, there are other alternatives, 1. Make a proper name when
    // we add a network interface by calling Add-VMNetworkAdapter and using the name as the unique
    // identifier to remove it. However, this was not done from the beginning, so it will not be
    // backward compatible. 2. Assume the network adapters are in the added order. However, hyper-v
    // Get-VMNetworkAdapter does not guarantee that. 3. Use the switch name to query the network
    // adapter. However, it might look like a unique identifier but actually it is not.
    power_shell->easy_run(
        {"Get-VMNetworkAdapter -VMName",
         name,
         "| Where-Object {$_.MacAddress -eq",
         // "Where-Object {$_.MacAddress -eq <mac_address>}" clause requires the string quoted and
         // no colon delimiter, for example "5254002CC58C"; whereas the "Set-VMNetworkAdapter
         // -StaticMacAddress <mac_address>" can accept unquoted and with colon delimiter like
         // 52:54:00:2C:C5:8B.
         quoted(QString::fromStdString(src_specs.default_mac_address).remove(':')),
         "} | Set-VMNetworkAdapter -StaticMacAddress",
         QString::fromStdString(desc.default_mac_address)},
        "Could not setup the default network adapter");

    assert(src_specs.extra_interfaces.size() == desc.extra_interfaces.size());
    for (size_t i = 0; i < src_specs.extra_interfaces.size(); ++i)
    {
        power_shell->easy_run(
            {"Get-VMNetworkAdapter -VMName",
             name,
             "| Where-Object {$_.MacAddress -eq",
             quoted(QString::fromStdString(src_specs.extra_interfaces[i].mac_address).remove(':')),
             "} | Set-VMNetworkAdapter -StaticMacAddress",
             QString::fromStdString(desc.extra_interfaces[i].mac_address)},
            "Could not setup the extra network adapter");
    }
}

mp::HyperVVirtualMachine::~HyperVVirtualMachine()
{
    top_catch_all(vm_name, [this]() {
        update_suspend_status = false;

        if (current_state() == State::running)
            suspend();
    });
}

void mp::HyperVVirtualMachine::start()
{
    state = State::starting;
    update_state();

    QString output_err;
    if (!power_shell->run({"Start-VM", "-Name", name}, nullptr, &output_err))
    {
        state = instance_state_for(power_shell.get(), name);
        update_state();
        throw StartException{vm_name, output_err.toStdString()};
    }
}

void mp::HyperVVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    std::unique_lock<std::mutex> lock{state_mutex};
    const auto present_state = current_state();

    try
    {
        check_state_for_shutdown(shutdown_policy);
    }
    catch (const VMStateIdempotentException& e)
    {
        mpl::log_message(mpl::Level::info, vm_name, e.what());
        return;
    }

    drop_ssh_session();

    if (shutdown_policy == ShutdownPolicy::Poweroff)
    {
        mpl::log(mpl::Level::info, vm_name, "Forcing shutdown");
        power_shell->run({"Stop-VM", "-Name", name, "-TurnOff"});
    }
    else
    {
        power_shell->run({"Stop-VM", "-Name", name});
    }

    state = State::off;

    // If it wasn't force, we wouldn't be here
    if (present_state == State::starting)
    {
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
    }

    update_state();
}

void mp::HyperVVirtualMachine::suspend()
{
    auto present_state = instance_state_for(power_shell.get(), name);

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        power_shell->run({"Save-VM", "-Name", name});

        drop_ssh_session();
        if (update_suspend_status)
        {
            state = State::suspended;
            update_state();
        }
    }
    else if (present_state == State::stopped)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring suspend issued while stopped"));
    }

    monitor->on_suspend();
}

mp::VirtualMachine::State mp::HyperVVirtualMachine::current_state()
{
    auto present_state = instance_state_for(power_shell.get(), name);

    if ((state == State::delayed_shutdown && present_state == State::running) ||
        state == State::starting)
        return state;

    state = present_state;
    return state;
}

int mp::HyperVVirtualMachine::ssh_port()
{
    return 22;
}

void mp::HyperVVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] { return state != State::off; };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance shutdown during start");
}

void mp::HyperVVirtualMachine::update_state()
{
    // Invalidate the management IP address on state update.
    if (current_state() == VirtualMachine::State::running)
    {
        // Cached IPs become stale when the guest is restarted from within. By resetting them here
        // we at least cover multipass's restart initiatives, which include state updates.
        mpl::log(mpl::Level::debug,
                 vm_name,
                 "Invalidating cached mgmt IP address upon state update");
        management_ip = std::nullopt;
    }
    monitor->persist_state_for(vm_name, state);
}

std::string mp::HyperVVirtualMachine::ssh_hostname(std::chrono::milliseconds /*timeout*/)
{
    return name.toStdString() + ".mshome.net";
}

std::string mp::HyperVVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

std::string mp::HyperVVirtualMachine::management_ipv4()
{
    if (!management_ip)
    {
        // Not using cached SSH session for this because a) the underlying functions do not
        // guarantee constness; b) we endure the penalty of creating a new session only when we
        // don't have the IP yet.
        auto result =
            remote_ip(VirtualMachine::ssh_hostname(), ssh_port(), ssh_username(), key_provider);
        if (result)
            management_ip.emplace(result.value());
    }
    return management_ip ? management_ip.value().as_string() : "UNKNOWN";
}

std::string mp::HyperVVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperVVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);

    power_shell->easy_run(
        {"Set-VMProcessor", "-VMName", name, "-Count", QString::number(num_cores)},
        "Could not update CPUs");
}

void mp::HyperVVirtualMachine::resize_memory(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    QStringList resize_cmd = {"Set-VMMemory",
                              "-VMName",
                              name,
                              "-StartupBytes",
                              QString::number(new_size.in_bytes())};
    power_shell->easy_run(resize_cmd, "Could not resize memory");
}

void mp::HyperVVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    // Resize the current disk layer, which will differ from the original image if there are
    // snapshots
    // clang-format off
    QStringList resize_cmd = {"Get-VM", "-VMName", name, "|", "Select-Object", "VMId", "|", "Get-VHD", "|",
                              "Resize-VHD", "-SizeBytes", QString::number(new_size.in_bytes())}; // clang-format on
    power_shell->easy_run(resize_cmd, "Could not resize disk");
}

void mp::HyperVVirtualMachine::add_network_interface(int /* not used on this backend */,
                                                     const std::string& default_mac_addr,
                                                     const NetworkInterface& extra_interface)
{
    desc.extra_interfaces.push_back(extra_interface);
    add_extra_net(*power_shell, name, extra_interface);
    add_extra_interface_to_instance_cloud_init(default_mac_addr, extra_interface);
}

mp::MountHandler::UPtr mp::HyperVVirtualMachine::make_native_mount_handler(
    const std::string& target,
    const mp::VMMount& mount)
{
    static const SmbManager smb_manager{};
    return std::make_unique<SmbMountHandler>(this,
                                             &key_provider,
                                             target,
                                             mount,
                                             instance_dir.absolutePath(),
                                             smb_manager);
}

void mp::HyperVVirtualMachine::remove_snapshots_from_backend() const
{
    // Get-VMSnapshot -VMName "YourVMName" | Remove-VMSnapshot
    power_shell->easy_run({"Get-VMSnapshot -VMName", name, "| Remove-VMSnapshot"},
                          "Could not remove the snapshots");
}

auto mp::HyperVVirtualMachine::make_specific_snapshot(const std::string& snapshot_name,
                                                      const std::string& comment,
                                                      const std::string& instance_id,
                                                      const VMSpecs& specs,
                                                      std::shared_ptr<Snapshot> parent)
    -> std::shared_ptr<Snapshot>
{
    assert(power_shell);
    return std::make_shared<HyperVSnapshot>(snapshot_name,
                                            comment,
                                            instance_id,
                                            specs,
                                            std::move(parent),
                                            name,
                                            *this,
                                            *power_shell);
}

auto mp::HyperVVirtualMachine::make_specific_snapshot(const QString& filename)
    -> std::shared_ptr<Snapshot>
{
    return std::make_shared<HyperVSnapshot>(filename, *this, desc, *power_shell);
}
