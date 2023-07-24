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

#include <multipass/exceptions/start_exception.h>
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

std::optional<mp::IPAddress> remote_ip(const std::string& host, int port) // clang-format off
try // clang-format on
{
    mp::SSHSession session{host, port};

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

// require rvalue ref for the error msg, to avoid confusion with output parameter in PS::run
void checked_ps_run(mp::PowerShell& ps, const QStringList& args, std::string&& error_msg)
{
    if (!ps.run(args))
        throw std::runtime_error{std::move(error_msg)};
}
} // namespace

mp::HyperVVirtualMachine::HyperVVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor)
    : BaseVirtualMachine{desc.vm_name},
      name{QString::fromStdString(desc.vm_name)},
      username{desc.ssh_username},
      image_path{desc.image.image_path},
      power_shell{std::make_unique<PowerShell>(vm_name)},
      monitor{&monitor}
{
    if (!power_shell->run({"Get-VM", "-Name", name}))
    {
        auto mem_size = QString::number(desc.mem_size.in_bytes()); /* format documented in `Help(New-VM)`, under
        `-MemoryStartupBytes` option; */

        checked_ps_run(*power_shell, {QString("$switch = Get-VMSwitch -Id %1").arg(default_switch_guid)},
                       "Could not find the default switch");

        checked_ps_run(*power_shell,
                       {"New-VM", "-Name", name, "-Generation", "2", "-VHDPath", '"' + desc.image.image_path + '"',
                        "-BootDevice", "VHD", "-SwitchName", "$switch.Name", "-MemoryStartupBytes", mem_size},
                       "Could not create VM");
        checked_ps_run(*power_shell, {"Set-VMFirmware", "-VMName", name, "-EnableSecureBoot", "Off"},
                       "Could not disable secure boot");
        checked_ps_run(*power_shell, {"Set-VMProcessor", "-VMName", name, "-Count", QString::number(desc.num_cores)},
                       "Could not configure VM processor");
        checked_ps_run(*power_shell, {"Add-VMDvdDrive", "-VMName", name, "-Path", '"' + desc.cloud_init_iso + '"'},
                       "Could not setup cloud-init drive");
        checked_ps_run(*power_shell, {"Set-VMMemory", "-VMName", name, "-DynamicMemoryEnabled", "$false"},
                       "Could not disable dynamic memory");

        setup_network_interfaces(desc.default_mac_address, desc.extra_interfaces);

        state = State::off;
    }
    else
    {
        state = instance_state_for(power_shell.get(), name);
    }
}

void mp::HyperVVirtualMachine::setup_network_interfaces(const std::string& default_mac_address,
                                                        const std::vector<NetworkInterface>& extra_interfaces)
{
    checked_ps_run(*power_shell,
                   {"Set-VMNetworkAdapter", "-VMName", name, "-StaticMacAddress",
                    QString::fromStdString('"' + default_mac_address + '"')},
                   "Could not setup default adapter");

    for (const auto& net : extra_interfaces)
    {
        const auto switch_ = '"' + QString::fromStdString(net.id) + '"';
        checked_ps_run(*power_shell, {"Get-VMSwitch", "-Name", switch_},
                       fmt::format("Could not find the device to connect to: no switch named \"{}\"", net.id));
        checked_ps_run(*power_shell,
                       {"Add-VMNetworkAdapter", "-VMName", name, "-SwitchName", switch_, "-StaticMacAddress",
                        QString::fromStdString('"' + net.mac_address + '"')},
                       fmt::format("Could not setup adapter for {}", net.id));
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
        throw StartException{vm_name, output.toStdString()};
}

void mp::HyperVVirtualMachine::stop()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    auto present_state = current_state();

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        power_shell->run({"Stop-VM", "-Name", name});
        state = State::stopped;
        ip = std::nullopt;
    }
    else if (present_state == State::starting)
    {
        power_shell->run({"Stop-VM", "-Name", name, "-TurnOff"});
        state = State::off;
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
        ip = std::nullopt;
    }
    else if (present_state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
    }

    update_state();
    lock.unlock();
}

void mp::HyperVVirtualMachine::shutdown()
{
    stop();
}

void mp::HyperVVirtualMachine::suspend()
{
    auto present_state = instance_state_for(power_shell.get(), name);

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        power_shell->run({"Save-VM", "-Name", name});

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
    return username;
}

std::string mp::HyperVVirtualMachine::management_ipv4()
{
    if (!ip)
    {
        auto result = remote_ip(VirtualMachine::ssh_hostname(), ssh_port());
        if (result)
            ip.emplace(result.value());
    }
    return ip ? ip.value().as_string() : "UNKNOWN";
}

std::string mp::HyperVVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperVVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout, std::bind(&HyperVVirtualMachine::ensure_vm_is_running, this));
}

void mp::HyperVVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);

    checked_ps_run(*power_shell, {"Set-VMProcessor", "-VMName", name, "-Count", QString::number(num_cores)},
                   "Could not update CPUs");
}

void mp::HyperVVirtualMachine::resize_memory(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    QStringList resize_cmd = {"Set-VMMemory", "-VMName", name, "-StartupBytes", QString::number(new_size.in_bytes())};
    checked_ps_run(*power_shell, resize_cmd, "Could not resize memory");
}

void mp::HyperVVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    QStringList resize_cmd = {"Resize-VHD", "-Path", image_path, "-SizeBytes", QString::number(new_size.in_bytes())};
    checked_ps_run(*power_shell, resize_cmd, "Could not resize disk");
}

mp::MountHandler::UPtr mp::HyperVVirtualMachine::make_native_mount_handler(const mp::SSHKeyProvider* ssh_key_provider,
                                                                           const std::string& target,
                                                                           const mp::VMMount& mount)
{
    return std::make_unique<SmbMountHandler>(this, ssh_key_provider, target, mount,
                                             QFileInfo{image_path}.absolutePath());
}
