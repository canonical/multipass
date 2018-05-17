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
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <fmt/format.h>

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QSysInfo>

#include <thread>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const QHash<QString, QString> cpu_to_arch{{"x86_64", "x86_64"}, {"arm", "arm"},   {"arm64", "aarch64"},
                                          {"i386", "i386"},     {"power", "ppc"}, {"power64", "ppc64le"},
                                          {"s390x", "s390x"}};

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
    args << "-qmp"
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

    process->setProgram("qemu-system-" + cpu_to_arch.value(QSysInfo::currentCpuArchitecture()));
    process->setArguments(args);

    mpl::log(mpl::Level::debug, desc.vm_name,
             fmt::format("process working dir '{}'", process->workingDirectory().toStdString()));
    mpl::log(mpl::Level::info, desc.vm_name, fmt::format("process program '{}'", process->program().toStdString()));
    mpl::log(mpl::Level::info, desc.vm_name,
             fmt::format("process arguments '{}'", process->arguments().join(", ").toStdString()));
    return process;
}

void remove_tap_device(const QString& tap_device_name)
{
    if (mp::utils::run_cmd_for_status("ip", {"addr", "show", tap_device_name}))
    {
        mp::utils::run_cmd_for_status("ip", {"link", "delete", tap_device_name});
    }
}

auto qmp_execute_json(const QString& cmd)
{
    QJsonObject qmp;
    qmp.insert("execute", cmd);
    return QJsonDocument(qmp).toJson();
}
}

mp::QemuVirtualMachine::QemuVirtualMachine(const VirtualMachineDescription& desc, optional<mp::IPAddress> address,
                                           const std::string& tap_device_name, DNSMasqServer& dnsmasq_server,
                                           VMStatusMonitor& monitor)
    : VirtualMachine{desc.key_provider, desc.vm_name},
      ip{address},
      is_legacy_ip{ip ? true : false},
      tap_device_name{tap_device_name},
      mac_addr{desc.mac_addr},
      username{desc.ssh_username},
      dnsmasq_server{&dnsmasq_server},
      monitor{&monitor},
      vm_process{make_qemu_process(desc, tap_device_name, mac_addr)}
{
    QObject::connect(vm_process.get(), &QProcess::started, [this]() {
        mpl::log(mpl::Level::info, vm_name, "process started");
        on_started();
    });
    QObject::connect(vm_process.get(), &QProcess::readyReadStandardOutput, [this]() {
        auto qmp_output = QJsonDocument::fromJson(vm_process->readAllStandardOutput()).object();
        auto event = qmp_output["event"];

        if (!event.isNull())
        {
            if (event.toString() == "RESET" && state != State::restarting)
            {
                mpl::log(mpl::Level::info, vm_name, "VM restarting");
                on_restart();
            }
            else if (event.toString() == "POWERDOWN")
            {
                mpl::log(mpl::Level::info, vm_name, "VM powering down");
            }
            else if (event.toString() == "SHUTDOWN")
            {
                mpl::log(mpl::Level::info, vm_name, "VM shut down");
            }
        }
    });

    QObject::connect(vm_process.get(), &QProcess::readyReadStandardError, [this]() {
        saved_error_msg = vm_process->readAllStandardError().data();
        mpl::log(mpl::Level::error, vm_name, saved_error_msg);
    });

    QObject::connect(vm_process.get(), &QProcess::stateChanged, [this](QProcess::ProcessState newState) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessState>();
        mpl::log(mpl::Level::info, vm_name, fmt::format("process state changed to {}", meta.valueToKey(newState)));
    });

    QObject::connect(vm_process.get(), &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessError>();
        mpl::log(mpl::Level::error, vm_name, fmt::format("process error occurred {}", meta.valueToKey(error)));
        on_error();
    });

    QObject::connect(vm_process.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     [this](int exitCode, QProcess::ExitStatus exitStatus) {
                         mpl::log(mpl::Level::info, vm_name,
                                  fmt::format("process finished with exit code {}", exitCode));
                         on_shutdown();
                     });
}

mp::QemuVirtualMachine::~QemuVirtualMachine()
{
    remove_tap_device(QString::fromStdString(tap_device_name));

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

    vm_process->write(qmp_execute_json("qmp_capabilities"));
}

void mp::QemuVirtualMachine::stop()
{
    shutdown();
}

void mp::QemuVirtualMachine::shutdown()
{
    if (state == State::running && vm_process->processId() > 0)
    {
        vm_process->write(qmp_execute_json("system_powerdown"));
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
    state = State::starting;
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

void mp::QemuVirtualMachine::on_restart()
{
    state = State::restarting;

    if (!is_legacy_ip)
        ip = nullopt;

    monitor->on_restart(vm_name);
}

void mp::QemuVirtualMachine::ensure_vm_is_running()
{
    QCoreApplication::processEvents();

    if (vm_process->state() == QProcess::NotRunning)
        throw mp::StartException(vm_name, saved_error_msg);
}

std::string mp::QemuVirtualMachine::ssh_hostname()
{
    if (!ip)
    {
        auto action = [this] {
            ensure_vm_is_running();
            auto result = dnsmasq_server->get_ip_for(mac_addr);
            if (result)
            {
                ip.emplace(result.value());
                return mp::utils::TimeoutAction::done;
            }
            else
            {
                return mp::utils::TimeoutAction::retry;
            }
        };
        auto on_timeout = [] { return std::runtime_error("failed to determine IP address"); };
        mp::utils::try_action_for(on_timeout, std::chrono::minutes(2), action);
    }

    return ip.value().as_string();
}

std::string mp::QemuVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::QemuVirtualMachine::ipv4()
{
    if (!ip)
    {
        auto result = dnsmasq_server->get_ip_for(mac_addr);
        if (result)
            ip.emplace(result.value());
        else
            return "UNKNOWN";
    }

    return ip.value().as_string();
}

std::string mp::QemuVirtualMachine::ipv6()
{
    return {};
}

void mp::QemuVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    auto process_vm_events = [this] { ensure_vm_is_running(); };

    mp::utils::wait_until_ssh_up(this, timeout, process_vm_events);
}

void mp::QemuVirtualMachine::wait_for_cloud_init(std::chrono::milliseconds timeout)
{
    auto process_vm_events = [this] { ensure_vm_is_running(); };

    mp::utils::wait_for_cloud_init(this, timeout, process_vm_events);
}
