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

#include "hyperkit_virtual_machine.h"

#include "vmprocess.h"

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/format.h>
#include <multipass/ip_address.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <shared/macos/backend_utils.h>
#include <shared/shared_backend_utils.h>

#include <QMetaObject>
#include <QString>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::HyperkitVirtualMachine::HyperkitVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor)
    : BaseVirtualMachine{State::off, desc.vm_name},
      monitor{&monitor},
      username{desc.ssh_username},
      desc{desc},
      update_shutdown_status{true}
{
    thread.setObjectName("HyperkitVirtualMachine thread");
}

mp::HyperkitVirtualMachine::~HyperkitVirtualMachine()
{
    update_shutdown_status = false;
    shutdown();
}

void mp::HyperkitVirtualMachine::start()
{
    if (state == State::off)
    {
        vm_process = std::make_unique<VMProcess>();

        vm_process->moveToThread(&thread);

        QObject::connect(vm_process.get(), &VMProcess::started, [=]() { on_start(); });
        QObject::connect(vm_process.get(), &VMProcess::stopped, [=](bool) { thread.quit(); });

        // cross-thread control
        QObject::connect(&thread, &QThread::started, vm_process.get(), [=]() { vm_process->start(desc); });
        QObject::connect(&thread, &QThread::finished, [=]() {
            if (update_shutdown_status)
            {
                on_shutdown();
            }
        });

        thread.start();
    }
}

void mp::HyperkitVirtualMachine::stop()
{
    if (state != State::off && state != State::stopped)
    {
        QMetaObject::invokeMethod(vm_process.get(), "stop");
        thread.wait(20000);
        update_state();
    }
}

void mp::HyperkitVirtualMachine::shutdown()
{
    stop();
}

void mp::HyperkitVirtualMachine::suspend()
{
    throw std::runtime_error("suspend is currently not supported");
}

void mp::HyperkitVirtualMachine::on_start()
{
    state = State::starting;
    update_state();
    monitor->on_resume();
}

void mp::HyperkitVirtualMachine::on_shutdown()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    if (state == State::starting)
    {
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
    }
    else
    {
        state = State::off;
    }

    ip = std::nullopt;
    update_state();
    lock.unlock();
    monitor->on_shutdown();
    vm_process.reset();
}

void mp::HyperkitVirtualMachine::ensure_vm_is_running()
{
    auto is_vm_running = [this] { return thread.isRunning(); };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, "Instance stopped while starting");
}

mp::VirtualMachine::State mp::HyperkitVirtualMachine::current_state()
{
    return state;
}

void mp::HyperkitVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

int mp::HyperkitVirtualMachine::ssh_port()
{
    return 22;
}

std::string mp::HyperkitVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    auto get_ip = [this]() -> std::optional<IPAddress> { return mp::backend::get_vmnet_dhcp_ip_for(vm_name); };

    return mp::backend::ip_address_for(this, get_ip, timeout);
}

std::string mp::HyperkitVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::HyperkitVirtualMachine::management_ipv4()
{
    if (ip)
        return ip->as_string();

    auto result = mp::backend::get_vmnet_dhcp_ip_for(vm_name);
    if (result)
    {
        ip.emplace(*result);
        return ip->as_string();
    }
    else
        return "UNKNOWN";
}

std::string mp::HyperkitVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperkitVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout, std::bind(&HyperkitVirtualMachine::ensure_vm_is_running, this));
}

void mp::HyperkitVirtualMachine::update_cpus(int num_cores)
{
    throw NotImplementedOnThisBackendException{"Instance modification"};
}

void mp::HyperkitVirtualMachine::resize_memory(const MemorySize& new_size)
{
    throw NotImplementedOnThisBackendException{"Instance modification"};
}

void mp::HyperkitVirtualMachine::resize_disk(const MemorySize& new_size)
{
    throw NotImplementedOnThisBackendException{"Instance modification"};
}
