/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Gerry Boland <gerry.boland@canonical.com>
 */

#include "hyperkit_virtual_machine.h"

#include "vmprocess.h"

#include <multipass/exceptions/start_exception.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <QEventLoop>
#include <QMetaObject>
#include <QString>
#include <QTimer>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::HyperkitVirtualMachine::HyperkitVirtualMachine(const VirtualMachineDescription& desc, VMStatusMonitor& monitor)
    : VirtualMachine{State::off, desc.vm_name},
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
        QObject::connect(vm_process.get(), &VMProcess::ip_address_found,
                         [=](std::string ip) { on_ip_address_found(ip); });

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
    if (state != State::off || state != State::stopped)
    {
        QMetaObject::invokeMethod(vm_process.get(), "stop");
        thread.wait(20000);
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
        state_wait.wait(lock, [this] { return state == State::off; });
    }
    else
    {
        state = State::off;
    }

    ip_address.clear();
    update_state();
    lock.unlock();
    monitor->on_shutdown();
    vm_process.reset();
}

void multipass::HyperkitVirtualMachine::on_ip_address_found(std::string ip)
{
    ip_address = ip;
}

void mp::HyperkitVirtualMachine::ensure_vm_is_running()
{
    std::lock_guard<decltype(state_mutex)> lock{state_mutex};
    if (!thread.isRunning())
    {
        // Have to set 'off' here so there is an actual state change to compare to for
        // the cond var's predicate
        state = State::off;
        state_wait.notify_all();
        throw mp::StartException(vm_name, "Instance stopped while starting");
    }
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

std::string mp::HyperkitVirtualMachine::ssh_hostname()
{
    return ipv4();
}

std::string mp::HyperkitVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::HyperkitVirtualMachine::ipv4()
{
    if (state == State::starting && ip_address.empty())
    {
        QEventLoop loop;
        QObject::connect(vm_process.get(), &VMProcess::ip_address_found, &loop, [this, &loop](std::string ip) {
            ip_address = ip;
            state = State::running;
            update_state();
            loop.quit();
        });
        QTimer::singleShot(40000, &loop, [&loop, this]() {
            mpl::log(mpl::Level::error, desc.vm_name, "Unable to determine IP address");
            state = State::unknown;
            update_state();
            loop.quit();
        });
        loop.exec();
    }
    return ip_address;
}

std::string mp::HyperkitVirtualMachine::ipv6()
{
    return {};
}

void mp::HyperkitVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout, std::bind(&HyperkitVirtualMachine::ensure_vm_is_running, this));
}
