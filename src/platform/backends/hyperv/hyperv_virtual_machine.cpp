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

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h> // TODO@snapshots drop
#include <multipass/exceptions/start_exception.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
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

QString quoted(const QString& str)
{
    return '"' + str + '"';
}

std::optional<mp::IPAddress> remote_ip(const std::string& host,
                                       int port,
                                       const std::string& username,
                                       const mp::SSHKeyProvider& key_provider) // clang-format off
try // clang-format on
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

    if (power_shell->run({"Get-VM", "-Name", name, "|", "Select-Object", "-ExpandProperty", "State"}, &state,
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

void delete_automatic_snapshots(mp::PowerShell* power_shell, const QString& name)
{
    power_shell->easy_run({"Get-VMCheckpoint -VMName",
                           name,
                           "| Where-Object { $_.IsAutomaticCheckpoint } | Remove-VMCheckpoint -Confirm:$false"},
                          "Could not delete existing automatic checkpoints");
}

void add_extra_net(mp::PowerShell& ps, const QString& name, const mp::NetworkInterface& net)
{
    const auto switch_ = '"' + QString::fromStdString(net.id) + '"';
    ps.easy_run({"Get-VMSwitch", "-Name", switch_},
                fmt::format("Could not find the device to connect to: no switch named \"{}\"", net.id));
    ps.easy_run({"Add-VMNetworkAdapter",
                 "-VMName",
                 name,
                 "-SwitchName",
                 switch_,
                 "-StaticMacAddress",
                 QString::fromStdString('"' + net.mac_address + '"')},
                fmt::format("Could not setup adapter for {}", net.id));
}
} // namespace

mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc,
                                               VMStatusMonitor& monitor,
                                               const SSHKeyProvider& key_provider,
                                               const mp::Path& instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir},
      desc{desc},
      name{QString::fromStdString(desc.vm_name)},
      power_shell{std::make_unique<PowerShell>(vm_name)},
      monitor{&monitor}
{
    if (!power_shell->run({"Get-VM", "-Name", name}))
    {
        auto mem_size = QString::number(desc.mem_size.in_bytes()); /* format documented in `Help(New-VM)`, under
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
        power_shell->easy_run({"Set-VMProcessor", "-VMName", name, "-Count", QString::number(desc.num_cores)},
                              "Could not configure VM processor");
        power_shell->easy_run({"Add-VMDvdDrive", "-VMName", name, "-Path", '"' + desc.cloud_init_iso + '"'},
                              "Could not setup cloud-init drive");
        power_shell->easy_run({"Set-VMMemory", "-VMName", name, "-DynamicMemoryEnabled", "$false"},
                              "Could not disable dynamic memory");

        setup_network_interfaces();

        state = State::off;
    }
    else
    {
        state = instance_state_for(power_shell.get(), name);
    }

    power_shell->easy_run({"Set-VM", "-Name", name, "-AutomaticCheckpointsEnabled", "$false"},
                          "Could not disable automatic snapshots"); // TODO move to new VMs only in a couple of releases
    delete_automatic_snapshots(power_shell.get(), name); // TODO drop in a couple of releases (going in on v1.13)
}

mp::HyperVVirtualMachine::HyperVVirtualMachine(const std::string& source_vm_name,
                                               const VirtualMachineDescription& desc,
                                               VMStatusMonitor& monitor,
                                               const SSHKeyProvider& key_provider,
                                               const Path& dest_instance_dir)
    : BaseVirtualMachine{desc.vm_name, key_provider, dest_instance_dir},
      desc{desc},
      name{QString::fromStdString(desc.vm_name)},
      power_shell{std::make_unique<PowerShell>(vm_name)},
      monitor{&monitor}
{
    // 1. Export-VM -Name vm1 -Path C:\ProgramData\Multipass\data\vault\instances\vm1-clone1
    power_shell->easy_run({"Export-VM", "-Name", QString::fromStdString(source_vm_name), "-Path", quoted(dest_instance_dir)},
                          "Could not export the source vm");
    // 2. $importedvm=Import-VM -Path 'C:\ProgramData\Multipass\data\vault\instances\vm1-clone1\vm1\Virtual
    // Machines\7735327A-A22F-4926-95A1-51757D650BB7.vmcx' -Copy -GenerateNewId -VhdDestinationPath
    // "C:\ProgramData\Multipass\data\vault\instances\vm1-clone1\"
    // 3. Rename-vm $importedvm -NewName vm1-clone1
    // 4. Remove-VMDvdDrive -VMName vm1-clone1 -ControllerNumber 0 -ControllerLocation 1
    // 5. Add-VMDvdDrive -VMName vm1-clone1 -Path
    // 'C:\ProgramData\Multipass\data\vault\instances\vm1-clone1\cloud-init-config.iso'
    // 6. Reset the default address, remove all original extra interfaces and add all the new ones when the
    // extra_interfaces vec is non-empty.
}

void mp::HyperVVirtualMachine::setup_network_interfaces()
{
    power_shell->easy_run({"Set-VMNetworkAdapter",
                           "-VMName",
                           name,
                           "-StaticMacAddress",
                           QString::fromStdString('"' + desc.default_mac_address + '"')},
                          "Could not setup default adapter");

    for (const auto& net : desc.extra_interfaces)
    {
        add_extra_net(*power_shell, name, net);
    }
}

mp::HyperVVirtualMachine::~HyperVVirtualMachine()
{
    update_suspend_status = false;

    if (current_state() == State::running)
        suspend();
}

void mp::HyperVVirtualMachine::start()
{
    state = State::starting;
    update_state();

    QString output;
    if (!power_shell->run({"Start-VM", "-Name", name}, &output))
    {
        state = instance_state_for(power_shell.get(), name);
        update_state();
        throw StartException{vm_name, output.toStdString()};
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
        mpl::log(mpl::Level::info, vm_name, e.what());
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

    ip = std::nullopt;
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

    if ((state == State::delayed_shutdown && present_state == State::running) || state == State::starting)
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
    if (!ip)
    {
        // Not using cached SSH session for this because a) the underlying functions do not guarantee constness;
        // b) we endure the penalty of creating a new session only when we don't have the IP yet.
        auto result = remote_ip(VirtualMachine::ssh_hostname(), ssh_port(), ssh_username(), key_provider);
        if (result)
            ip.emplace(result.value());
    }
    return ip ? ip.value().as_string() : "UNKNOWN";
}

std::string mp::HyperVVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperVVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);

    power_shell->easy_run({"Set-VMProcessor", "-VMName", name, "-Count", QString::number(num_cores)},
                          "Could not update CPUs");
}

void mp::HyperVVirtualMachine::resize_memory(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    QStringList resize_cmd = {"Set-VMMemory", "-VMName", name, "-StartupBytes", QString::number(new_size.in_bytes())};
    power_shell->easy_run(resize_cmd, "Could not resize memory");
}

void mp::HyperVVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    // Resize the current disk layer, which will differ from the original image if there are snapshots
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

mp::MountHandler::UPtr mp::HyperVVirtualMachine::make_native_mount_handler(const std::string& target,
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

auto mp::HyperVVirtualMachine::make_specific_snapshot(const std::string& snapshot_name,
                                                      const std::string& comment,
                                                      const std::string& instance_id,
                                                      const VMSpecs& specs,
                                                      std::shared_ptr<Snapshot> parent) -> std::shared_ptr<Snapshot>
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

auto mp::HyperVVirtualMachine::make_specific_snapshot(const QString& filename) -> std::shared_ptr<Snapshot>
{
    return std::make_shared<HyperVSnapshot>(filename, *this, desc, *power_shell);
}

std::shared_ptr<mp::Snapshot> mp::HyperVVirtualMachine::make_specific_snapshot(const QString& filename,
                                                                               const VMSpecs& src_specs,
                                                                               const VMSpecs& dest_specs,
                                                                               const std::string& src_vm_name)
{
    return std::make_shared<HyperVSnapshot>(filename, src_specs, dest_specs, src_vm_name, *this, desc, *power_shell);
}
