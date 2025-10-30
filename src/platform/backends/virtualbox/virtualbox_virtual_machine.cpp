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

#include "virtualbox_virtual_machine.h"
#include "virtualbox_snapshot.h"

#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/ip_address.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/standard_paths.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_status_monitor.h>

#include <fmt/format.h>

#include <QProcess>
#include <QRegularExpression>
#include <QtNetwork/QTcpServer>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;
namespace fs = std::filesystem;

namespace
{
auto instance_state_for(const QString& name)
{
    QRegularExpression vmstate_re("VMState=\"([a-z]+)\"");

    QProcess vminfo;
    vminfo.start("VBoxManage", {"showvminfo", name, "--machinereadable"});
    auto success = vminfo.waitForFinished();
    if (!success || vminfo.exitStatus() != QProcess::NormalExit)
    {
        throw std::runtime_error(
            fmt::format("Failed to run VBoxManage: {}", vminfo.errorString().toStdString()));
    }
    auto vminfo_output = QString::fromUtf8(vminfo.readAllStandardOutput());
    auto vmstate_match = vmstate_re.match(vminfo_output);

    if (vmstate_match.hasMatch())
    {
        auto state = vmstate_match.captured(1);

        mpl::trace(name.toStdString(), "Got VMState: {}", state.toStdString());

        if (state == "starting" || state == "restoring")
        {
            return mp::VirtualMachine::State::starting;
        }
        else if (state == "running" || state == "paused" || state == "onlinesnapshotting" ||
                 state == "stopping")
        {
            return mp::VirtualMachine::State::running;
        }
        else if (state == "saving")
        {
            return mp::VirtualMachine::State::suspending;
        }
        else if (state == "saved")
        {
            return mp::VirtualMachine::State::suspended;
        }
        else if (state == "poweroff" || state == "aborted")
        {
            return mp::VirtualMachine::State::stopped;
        }

        mpl::error(name.toStdString(),
                   "Failed to parse instance state: {}",
                   vmstate_match.captured().toStdString());
    }
    else if (vminfo.exitCode() == 0)
    {
        mpl::error(name.toStdString(),
                   "Failed to parse info output: {}",
                   vminfo_output.toStdString());
    }

    return mp::VirtualMachine::State::unknown;
}

QStringList extra_net_args(int index, const mp::NetworkInterface& net)
{
    QString iface_index_str = QString::number(index);

    return QStringList{"--nic" + iface_index_str,
                       "bridged",
                       "--nictype" + iface_index_str,
                       "virtio",
                       "--macaddress" + iface_index_str,
                       QString::fromStdString(net.mac_address).remove(':'),
                       "--bridgeadapter" + iface_index_str,
                       QString::fromStdString(mp::platform::reinterpret_interface_id(net.id))};
}

QStringList networking_arguments(const mp::VirtualMachineDescription& desc)
{
    QStringList arguments{"--nic1",
                          "nat",
                          "--nictype1",
                          "virtio",
                          "--macaddress1",
                          QString::fromStdString(desc.default_mac_address).remove(':')};

    for (size_t i = 0; i < desc.extra_interfaces.size(); ++i)
    {
        arguments += extra_net_args(i + 2, desc.extra_interfaces[i]);
    }

    return arguments;
}

QStringList modifyvm_arguments(const mp::VirtualMachineDescription& desc, const QString& vm_name)
{
    const QString& tmp = MP_STDPATHS.writableLocation(mp::StandardPaths::TempLocation);
    const QString& log_file = QString("%1/%2.log").arg(tmp).arg(vm_name);
    QStringList modify_arguments{"modifyvm",    vm_name,
                                 "--cpus",      QString::number(desc.num_cores),
                                 "--memory",    QString::number(desc.mem_size.in_megabytes()),
                                 "--boot1",     "disk",
                                 "--boot2",     "none",
                                 "--boot3",     "none",
                                 "--boot4",     "none",
                                 "--acpi",      "on",
                                 "--firmware",  "efi",
                                 "--rtcuseutc", "on",
                                 "--audio",     "none",
                                 "--uart1",     "0x3f8",
                                 "4",           "--uartmode1",
                                 "file",        log_file};
    modify_arguments += networking_arguments(desc);

    return modify_arguments;
}

void update_mac_addresses_of_network_adapters(const mp::VirtualMachineDescription& desc,
                                              const QString& vm_name)
{
    mpu::process_log_on_error("VBoxManage",
                              {"modifyvm",
                               vm_name,
                               "--macaddress1",
                               QString::fromStdString(desc.default_mac_address).remove(':')},
                              "Could not update the default network adapter address of: {}",
                              vm_name);
    for (size_t i = 0; i < desc.extra_interfaces.size(); ++i)
    {
        const size_t current_adapter_number = i + 2;
        mpu::process_log_on_error(
            "VBoxManage",
            {"modifyvm",
             vm_name,
             "--macaddress" + QString::number(current_adapter_number),
             QString::fromStdString(desc.extra_interfaces[i].mac_address).remove(':')},
            "Could not update the network adapter address of: {}",
            vm_name);
    }
}

} // namespace

mp::VirtualBoxVirtualMachine::VirtualBoxVirtualMachine(const VirtualMachineDescription& desc,
                                                       VMStatusMonitor& monitor,
                                                       const SSHKeyProvider& key_provider,
                                                       const mp::Path& instance_dir_qstr)
    : VirtualBoxVirtualMachine(desc, monitor, key_provider, instance_dir_qstr, true)
{
    if (desc.extra_interfaces.size() > 7)
    {
        throw std::runtime_error("VirtualBox does not support more than 8 interfaces");
    }

    if (!mpu::process_log_on_error("VBoxManage",
                                   {"showvminfo", name, "--machinereadable"},
                                   "Could not get instance info: {}",
                                   name))
    {
        const fs::path instances_dir = fs::path{instance_dir_qstr.toStdString()}.parent_path();
        mpu::process_throw_on_error("VBoxManage",
                                    {"createvm",
                                     "--name",
                                     name,
                                     "--basefolder",
                                     QString::fromStdString(instances_dir.string()),
                                     "--ostype",
                                     "ubuntu_64",
                                     "--register"},
                                    "Could not create VM: {}",
                                    name);

        mpu::process_throw_on_error("VBoxManage",
                                    modifyvm_arguments(desc, name),
                                    "Could not modify VM: {}",
                                    name);

        mpu::process_throw_on_error(
            "VBoxManage",
            {"storagectl", name, "--add", "sata", "--name", "SATA_0", "--portcount", "2"},
            "Could not modify VM: {}",
            name);

        mpu::process_throw_on_error("VBoxManage",
                                    {"storageattach",
                                     name,
                                     "--storagectl",
                                     "SATA_0",
                                     "--port",
                                     "0",
                                     "--device",
                                     "0",
                                     "--type",
                                     "hdd",
                                     "--medium",
                                     desc.image.image_path},
                                    "Could not storageattach HDD: {}",
                                    name);

        mpu::process_throw_on_error("VBoxManage",
                                    {"storageattach",
                                     name,
                                     "--storagectl",
                                     "SATA_0",
                                     "--port",
                                     "1",
                                     "--device",
                                     "0",
                                     "--type",
                                     "dvddrive",
                                     "--medium",
                                     desc.cloud_init_iso},
                                    "Could not storageattach DVD: {}",
                                    name);

        state = State::off;
    }
    else
    {
        state = instance_state_for(name);
    }
}

mp::VirtualBoxVirtualMachine::VirtualBoxVirtualMachine(const std::string& source_vm_name,
                                                       const VirtualMachineDescription& desc,
                                                       VMStatusMonitor& monitor,
                                                       const SSHKeyProvider& key_provider,
                                                       const Path& dest_instance_dir)
    : VirtualBoxVirtualMachine(desc, monitor, key_provider, dest_instance_dir, true)
{
    const fs::path instances_dir = fs::path{dest_instance_dir.toStdString()}.parent_path();

    // 1. clone the vm with certain options and mode. --mode value is all, which copies all snapshot
    // history and it always includes the base disk whereas machine mode only copies the current
    // differencing disk when the vm is snapshoted. --options has keepdisknames and keepallmacs,
    // keepdisknames means disk file name will be kept instead of using <vm_name>.vdi, keepallmacs
    // implies that the mac addresses of network adapters will not be generated by virtualbox
    // because they will be overwritten by multipass generated mac addresses anyway.
    mpu::process_throw_on_error("VBoxManage",
                                {"clonevm",
                                 QString::fromStdString(source_vm_name),
                                 "--name",
                                 name,
                                 "--register",
                                 "--basefolder",
                                 QString::fromStdString(instances_dir.string()),
                                 "--mode",
                                 "all",
                                 "--options",
                                 "keepdisknames,keepallmacs"},
                                "Could not clone VM: {}",
                                QString::fromStdString(source_vm_name));

    // 2. remove the old cloud-init file from the vm
    mpu::process_throw_on_error("VBoxManage",
                                {"storageattach",
                                 name,
                                 "--storagectl",
                                 "SATA_0",
                                 "--port",
                                 "1",
                                 "--device",
                                 "0",
                                 "--type",
                                 "dvddrive",
                                 "--medium",
                                 "none"},
                                "Could not remove the cloud-init file from: {}",
                                name);
    // 3. attach the new cloud-file to the vm
    mpu::process_throw_on_error("VBoxManage",
                                {"storageattach",
                                 name,
                                 "--storagectl",
                                 "SATA_0",
                                 "--port",
                                 "1",
                                 "--device",
                                 "0",
                                 "--type",
                                 "dvddrive",
                                 "--medium",
                                 desc.cloud_init_iso},
                                "Could not attach the cloud-init file to: {}",
                                name);
    // 4. reset the mac addresses of vm to the spec addres
    update_mac_addresses_of_network_adapters(desc, name);
    remove_snapshots_from_backend();
}

mp::VirtualBoxVirtualMachine::VirtualBoxVirtualMachine(const VirtualMachineDescription& desc,
                                                       VMStatusMonitor& monitor,
                                                       const SSHKeyProvider& key_provider,
                                                       const mp::Path& instance_dir_qstr,
                                                       bool /*is_internal*/)
    : BaseVirtualMachine{desc.vm_name, key_provider, instance_dir_qstr},
      desc{desc},
      name{QString::fromStdString(desc.vm_name)},
      monitor{&monitor}
{
}

mp::VirtualBoxVirtualMachine::~VirtualBoxVirtualMachine()
{
    top_catch_all(vm_name, [this]() {
        update_suspend_status = false;

        if (current_state() == State::running)
            suspend();
    });
}

void mp::VirtualBoxVirtualMachine::start()
{
    state = State::starting;
    update_state();

    mpu::process_throw_on_error("VBoxManage",
                                {"startvm", name, "--type", "headless"},
                                "Could not start VM: {}",
                                name);
}

void mp::VirtualBoxVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
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
        mpl::info(vm_name, "Forcing shutdown");
        // virtualbox needs the discardstate command to shutdown in the suspend state, it discards
        // the saved state of the vm, which is akin to resetting it to the off state without a
        // proper shutdown process
        if (state == State::suspended)
        {
            mpu::process_throw_on_error("VBoxManage",
                                        {"discardstate", name},
                                        "Could not power VM off: {}",
                                        name);
        }
        else
        {
            mpu::process_throw_on_error("VBoxManage",
                                        {"controlvm", name, "poweroff"},
                                        "Could not power VM off: {}",
                                        name);
        }
    }
    else
    {
        mpu::process_throw_on_error("VBoxManage",
                                    {"controlvm", name, "acpipowerbutton"},
                                    "Could not stop VM: {}",
                                    name);
    }

    state = State::stopped;

    // If it wasn't force, we wouldn't be here
    if (present_state == State::starting)
    {
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
    }

    port = std::nullopt;
    update_state();
}

void mp::VirtualBoxVirtualMachine::suspend()
{
    auto present_state = instance_state_for(name);

    if (present_state == State::running || present_state == State::delayed_shutdown)
    {
        mpu::process_throw_on_error("VBoxManage",
                                    {"controlvm", name, "savestate"},
                                    "Could not suspend VM: {}",
                                    name);

        drop_ssh_session();
        if (update_suspend_status)
        {
            state = State::suspended;
            update_state();
        }
    }
    else if (present_state == State::stopped)
    {
        mpl::info(vm_name, "Ignoring suspend issued while stopped");
    }

    monitor->on_suspend();
}

mp::VirtualMachine::State mp::VirtualBoxVirtualMachine::current_state()
{
    auto present_state = instance_state_for(name);

    if ((state == State::delayed_shutdown && present_state == State::running) ||
        state == State::starting)
        return state;

    state = present_state;
    if (state == State::suspended || state == State::suspending)
        drop_ssh_session();

    return state;
}

int mp::VirtualBoxVirtualMachine::ssh_port()
{
    if (!port)
    {
        QTcpServer socket;
        if (!socket.listen(QHostAddress("127.0.0.1")))
        {
            throw std::runtime_error(fmt::format("Could not find a port available to listen on: {}",
                                                 socket.errorString()));
        }

        mpu::process_log_on_error("VBoxManage",
                                  {"controlvm", name, "natpf1", "delete", "ssh"},
                                  "Could not delete SSH port forwarding: {}",
                                  name);

        mpu::process_throw_on_error(
            "VBoxManage",
            {"controlvm",
             name,
             "natpf1",
             QString::fromStdString(fmt::format("ssh,tcp,,{},,22", socket.serverPort()))},
            "Could not add SSH port forwarding: {}",
            name);

        port.emplace(socket.serverPort());
    }
    return *port;
}

void mp::VirtualBoxVirtualMachine::ensure_vm_is_running()
{
    ensure_vm_is_running_for();
}

void mp::VirtualBoxVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

std::string mp::VirtualBoxVirtualMachine::ssh_hostname(std::chrono::milliseconds /*timeout*/)
{
    return "127.0.0.1";
}

std::string mp::VirtualBoxVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

auto mp::VirtualBoxVirtualMachine::management_ipv4() -> std::optional<IPAddress>
{
    return std::nullopt;
}

auto mp::VirtualBoxVirtualMachine::get_all_ipv4() -> std::vector<IPAddress>
{
    using namespace std;

    const auto internal_ip = IPAddress{"10.0.2.15"};
    auto all_ipv4 = BaseVirtualMachine::get_all_ipv4();
    all_ipv4.erase(remove(begin(all_ipv4), end(all_ipv4), internal_ip), end(all_ipv4));

    return all_ipv4;
}

void mp::VirtualBoxVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);

    mpu::process_throw_on_error("VBoxManage",
                                {"modifyvm", name, "--cpus", QString::number(num_cores)},
                                "Could not update CPUs: {}",
                                name);
}

void mp::VirtualBoxVirtualMachine::resize_memory(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    mpu::process_throw_on_error(
        "VBoxManage",
        {"modifyvm", name, "--memory", QString::number(new_size.in_megabytes())},
        "Could not update memory: {}",
        name);
}

void mp::VirtualBoxVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size.in_bytes() > 0);

    mpu::process_throw_on_error(
        "VBoxManage",
        {"modifyhd", desc.image.image_path, "--resizebyte", QString::number(new_size.in_bytes())},
        "Could not resize image: {}",
        name);
}

void mp::VirtualBoxVirtualMachine::add_network_interface(int index,
                                                         const std::string& default_mac_addr,
                                                         const NetworkInterface& extra_interface)
{
    auto arguments = QStringList{"modifyvm", name} + extra_net_args(index + 2, extra_interface);

    mpu::process_throw_on_error("VBoxManage",
                                arguments,
                                "Could not add network interface: {}",
                                name);
    add_extra_interface_to_instance_cloud_init(default_mac_addr, extra_interface);
}

void mp::VirtualBoxVirtualMachine::remove_snapshots_from_backend() const
{
    // Name: @s1 (UUID: 93a6a9ba-9223-4b77-a8cf-80213439aaae)
    // Description: snapshot1:
    //    Name: @s2 (UUID: 871d6b85-d11c-4969-8433-c4a143dba4d8)
    //    Description: snapshot2:
    //        Name: @s3 (UUID: c4800b70-1e50-4b84-b430-1856437fe967)
    //        Description: snapshot3:

    const QString snap_shot_list = QString::fromStdString(
        MP_UTILS.run_cmd_for_output("VBoxManage", {"snapshot", name, "list"}));
    const QRegularExpression uuid_key_value_regex(R"(UUID: ([\w-]+))");
    QRegularExpressionMatchIterator iter = uuid_key_value_regex.globalMatch(snap_shot_list);

    QStringList uuid_list;
    while (iter.hasNext())
    {
        QRegularExpressionMatch match = iter.next();
        // captured(0) is the entire match which contains UUID key and value, captured(1) is the
        // first regex capture group which is the value of the UUID only
        uuid_list.append(match.captured(1));
    }

    for (const auto& uuid : uuid_list)
    {
        mpu::process_throw_on_error("VBoxManage",
                                    {"snapshot", name, "delete", uuid},
                                    "Could not delete snapshot: {}",
                                    name);
    }
}

auto multipass::VirtualBoxVirtualMachine::make_specific_snapshot(const QString& filename)
    -> std::shared_ptr<Snapshot>
{
    return std::make_shared<VirtualBoxSnapshot>(filename, *this, desc);
}

auto multipass::VirtualBoxVirtualMachine::make_specific_snapshot(const std::string& snapshot_name,
                                                                 const std::string& comment,
                                                                 const std::string& instance_id,
                                                                 const multipass::VMSpecs& specs,
                                                                 std::shared_ptr<Snapshot> parent)
    -> std::shared_ptr<Snapshot>
{
    return std::make_shared<VirtualBoxSnapshot>(snapshot_name,
                                                comment,
                                                instance_id,
                                                std::move(parent),
                                                name,
                                                specs,
                                                *this);
}
