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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "qemu_virtual_machine.h"
#include "dnsmasq_server.h"
#include <multipass/exceptions/start_exception.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <thread>

namespace mp = multipass;

namespace
{
auto make_qemu_process(const mp::VirtualMachineDescription& desc, const std::string& tap_device_name,
                       const std::string& mac_addr)
{
    if (!QFile::exists(desc.image.image_path) || !QFile::exists(desc.cloud_init_iso))
    {
        throw std::runtime_error("cannot start VM without an image");
    }

    auto mem_size = QString::fromStdString(desc.mem_size);
    if (mem_size.endsWith("B"))
    {
        mem_size.chop(1);
    }

    QStringList args{"--enable-kvm"};
    // The VM image itself
    args << "-hda" << desc.image.image_path;
    // For the cloud-init configuration
    args << "-drive" << QString{"file="} + desc.cloud_init_iso + QString{",if=virtio,format=raw"};
    // Number of cpu cores
    args << "-smp" << QString::number(desc.num_cores);
    // Memory to use for VM
    args << "-m" << mem_size;
    // Create a virtual NIC in the VM
    args << "-device" << QString("virtio-net-pci,netdev=hostnet0,id=net0,mac=%1").arg(QString::fromStdString(mac_addr));
    // Create tap device to connect to virtual bridge
    args << "-netdev";
    args << QString("tap,id=hostnet0,ifname=%1,script=no,downscript=no").arg(QString::fromStdString(tap_device_name));
    // Control interface
    args << "-monitor"
         << "stdio";
    // No console
    args << "-chardev"
         // TODO Read and log machine output when verbose
         << "null,id=char0"
         << "-serial"
         << "chardev:char0"
         // TODO Add a debugging mode with access to console
         << "-nographic";

    auto process = std::make_unique<QProcess>();
    auto snap = qgetenv("SNAP");
    if (!snap.isEmpty())
    {
        process->setWorkingDirectory(snap.append("/qemu"));
    }
    qDebug() << "QProcess::workingDirectory" << process->workingDirectory();
    process->setProgram("qemu-system-x86_64");
    qDebug() << "QProcess::program" << process->program();
    process->setArguments(args);
    qDebug() << "QProcess::arguments" << process->arguments();

    return process;
}
}

mp::QemuVirtualMachine::QemuVirtualMachine(const VirtualMachineDescription& desc, optional<mp::IPAddress> address,
                                           const std::string& tap_device_name, const std::string& mac_addr,
                                           DNSMasqServer& dnsmasq_server, VMStatusMonitor& monitor)
    : state{State::off},
      ip{address},
      tap_device_name{tap_device_name},
      mac_addr{mac_addr},
      vm_name{desc.vm_name},
      dnsmasq_server{&dnsmasq_server},
      monitor{&monitor},
      vm_process{make_qemu_process(desc, tap_device_name, mac_addr)}
{
    QObject::connect(vm_process.get(), &QProcess::started, [this]() {
        qDebug() << "QProcess::started";
        on_started();
    });
    QObject::connect(vm_process.get(), &QProcess::readyReadStandardOutput,
                     [this]() { qDebug("qemu.out: %s", vm_process->readAllStandardOutput().data()); });

    QObject::connect(vm_process.get(), &QProcess::readyReadStandardError, [this]() {
        saved_error_msg = vm_process->readAllStandardError().data();
        qDebug("qemu.err: %s", saved_error_msg.c_str());
    });

    QObject::connect(vm_process.get(), &QProcess::stateChanged, [](QProcess::ProcessState newState) {
        qDebug() << "QProcess::stateChanged"
                 << "newState" << newState;
    });

    QObject::connect(vm_process.get(), &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        qDebug() << "QProcess::errorOccurred"
                 << "error" << error;
        on_error();
    });

    QObject::connect(vm_process.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     [this](int exitCode, QProcess::ExitStatus exitStatus) {
                         qDebug() << "QProcess::finished"
                                  << "exitCode" << exitCode << "exitStatus" << exitStatus;
                         on_shutdown();
                     });
}

mp::QemuVirtualMachine::~QemuVirtualMachine()
{
    shutdown();
}

void mp::QemuVirtualMachine::start()
{
    if (state == State::running)
        return;

    vm_process->start();
    auto started = vm_process->waitForStarted();

    if (!started)
        throw std::runtime_error("failed to start qemu instance");
}

void mp::QemuVirtualMachine::stop()
{
    shutdown();
}

void mp::QemuVirtualMachine::shutdown()
{
    if (state == State::running && vm_process->processId() > 0)
    {
        vm_process->write("system_powerdown\n");
        vm_process->waitForFinished();
    }
}

mp::VirtualMachine::State mp::QemuVirtualMachine::current_state()
{
    return state;
}

int mp::QemuVirtualMachine::ssh_port()
{
    return 22;
}

void mp::QemuVirtualMachine::on_started()
{
    state = State::running;
    monitor->on_resume();
}

void mp::QemuVirtualMachine::on_error()
{
    state = State::off;
}

void mp::QemuVirtualMachine::on_shutdown()
{
    state = State::off;
    monitor->on_shutdown();
}

std::string mp::QemuVirtualMachine::ssh_hostname()
{
    return ipv4();
}

std::string mp::QemuVirtualMachine::ipv4()
{
    if (!ip)
    {
        using namespace std::literals::chrono_literals;

        auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(2);

        while (std::chrono::steady_clock::now() < deadline)
        {
            QCoreApplication::processEvents();

            if (vm_process->state() == QProcess::NotRunning)
                throw mp::StartException(vm_name, saved_error_msg);

            auto result = dnsmasq_server->get_ip_for(mac_addr);

            if (result)
            {
                ip.emplace(result.value());
                return ip.value().as_string();
            }

            std::this_thread::sleep_for(1s);
        }

        throw std::runtime_error("failed to determine IP address");
    }
    else
        return ip.value().as_string();
}

std::string mp::QemuVirtualMachine::ipv6()
{
    return {};
}

void mp::QemuVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    auto precondition_check = [this]() {
        QCoreApplication::processEvents();
        if (vm_process->state() == QProcess::NotRunning)
            throw mp::StartException(vm_name, saved_error_msg);
    };

    SSHSession::wait_until_ssh_up(ssh_hostname(), ssh_port(), timeout, precondition_check);
}
