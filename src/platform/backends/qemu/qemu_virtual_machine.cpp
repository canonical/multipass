/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include "qemu_virtual_machine.h"
#include "qemu_mount_handler.h"
#include "qemu_vm_process_spec.h"
#include "qemu_vmstate_process_spec.h"

#include <shared/qemu_img_utils/qemu_img_utils.h>
#include <shared/shared_backend_utils.h>

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/process/simple_process_spec.h>
#include <multipass/utils.h>
#include <multipass/vm_mount.h>
#include <multipass/vm_status_monitor.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QUuid>

#include <cassert>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace std::chrono_literals;

namespace
{
constexpr auto suspend_tag = "suspend";
constexpr auto machine_type_key = "machine_type";
constexpr auto arguments_key = "arguments";

bool use_cdrom_set(const QJsonObject& metadata)
{
    return metadata.contains("use_cdrom") && metadata["use_cdrom"].toBool();
}

QString get_vm_machine(const QJsonObject& metadata)
{
    QString machine;
    if (metadata.contains(machine_type_key))
    {
        machine = metadata[machine_type_key].toString();
    }
    return machine;
}

QStringList get_arguments(const QJsonObject& metadata)
{
    QStringList args;
    if (metadata.contains(arguments_key) && metadata[arguments_key].type() == QJsonValue::Array)
    {
        auto array = metadata[arguments_key].toArray();
        for (const QJsonValueRef val : array)
        {
            args.push_back(val.toString());
        }
    }
    return args;
}

auto make_qemu_process(const mp::VirtualMachineDescription& desc, const std::optional<QJsonObject>& resume_metadata,
                       const std::unordered_map<std::string, std::pair<std::string, QStringList>> mount_args,
                       const QStringList& platform_args)
{
    if (!QFile::exists(desc.image.image_path) || !QFile::exists(desc.cloud_init_iso))
    {
        throw std::runtime_error("cannot start VM without an image");
    }

    std::optional<mp::QemuVMProcessSpec::ResumeData> resume_data;
    if (resume_metadata)
    {
        const auto& data = resume_metadata.value();
        resume_data = mp::QemuVMProcessSpec::ResumeData{suspend_tag, get_vm_machine(data), use_cdrom_set(data),
                                                        get_arguments(data)};
    }

    auto process_spec = std::make_unique<mp::QemuVMProcessSpec>(desc, platform_args, mount_args, resume_data);
    auto process = mp::platform::make_process(std::move(process_spec));

    mpl::log(mpl::Level::debug, desc.vm_name, fmt::format("process working dir '{}'", process->working_directory()));
    mpl::log(mpl::Level::info, desc.vm_name, fmt::format("process program '{}'", process->program()));
    mpl::log(mpl::Level::info, desc.vm_name, fmt::format("process arguments '{}'", process->arguments().join(", ")));
    return process;
}

auto qmp_execute_json(const QString& cmd)
{
    QJsonObject qmp;
    qmp.insert("execute", cmd);
    return QJsonDocument(qmp).toJson();
}

auto hmc_to_qmp_json(const QString& command_line)
{
    auto qmp = QJsonDocument::fromJson(qmp_execute_json("human-monitor-command")).object();

    QJsonObject cmd_line;
    cmd_line.insert("command-line", command_line);

    qmp.insert("arguments", cmd_line);

    return QJsonDocument(qmp).toJson();
}

bool instance_image_has_snapshot(const mp::Path& image_path)
{
    auto process =
        mp::platform::make_process(mp::simple_process_spec("qemu-img", QStringList{"snapshot", "-l", image_path}));
    auto process_state = process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Internal error: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(), process->read_all_standard_error()));
    }

    auto output = process->read_all_standard_output().split('\n');

    for (const auto& line : output)
    {
        if (line.contains(suspend_tag))
        {
            return true;
        }
    }

    return false;
}

auto get_qemu_machine_type(const QStringList& platform_args)
{
    QTemporaryFile dump_file;
    if (!dump_file.open())
    {
        return QString();
    }

    auto process_spec = std::make_unique<mp::QemuVmStateProcessSpec>(dump_file.fileName(), platform_args);
    auto process = mp::platform::make_process(std::move(process_spec));
    auto process_state = process->execute();

    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(
            fmt::format("Internal error: qemu-system-{} failed getting vmstate ({}) with output:\n{}", HOST_ARCH,
                        process_state.failure_message(), process->read_all_standard_error()));
    }

    auto vmstate = QJsonDocument::fromJson(dump_file.readAll()).object();

    auto machine_type = vmstate["vmschkmachine"].toObject()["Name"].toString();
    return machine_type;
}

auto generate_metadata(const QStringList& platform_args, const QStringList& proc_args)
{
    QJsonObject metadata;
    metadata[machine_type_key] = get_qemu_machine_type(platform_args);
    metadata[arguments_key] = QJsonArray::fromStringList(proc_args);
    return metadata;
}
} // namespace

mp::QemuVirtualMachine::QemuVirtualMachine(const VirtualMachineDescription& desc, QemuPlatform* qemu_platform,
                                           VMStatusMonitor& monitor)
    : BaseVirtualMachine{instance_image_has_snapshot(desc.image.image_path) ? State::suspended : State::off,
                         desc.vm_name},
      desc{desc},
      mac_addr{desc.default_mac_address},
      username{desc.ssh_username},
      qemu_platform{qemu_platform},
      monitor{&monitor}
{
    QObject::connect(
        this, &QemuVirtualMachine::on_delete_memory_snapshot, this,
        [this] {
            mpl::log(mpl::Level::debug, vm_name, fmt::format("Deleted memory snapshot"));
            vm_process->write(hmc_to_qmp_json("delvm " + QString::fromStdString(suspend_tag)));
            is_starting_from_suspend = false;
        },
        Qt::QueuedConnection);

    // The following is the actual code to reset the network via QMP if an IP address is not obtained after
    // starting from suspend.  This will probably be deprecated in the future.
    QObject::connect(
        this, &QemuVirtualMachine::on_reset_network, this,
        [this] {
            mpl::log(mpl::Level::debug, vm_name, fmt::format("Resetting the network"));

            auto qmp = QJsonDocument::fromJson(qmp_execute_json("set_link")).object();
            QJsonObject args;
            args.insert("name", "virtio-net-pci.0");
            args.insert("up", false);
            qmp.insert("arguments", args);

            vm_process->write(QJsonDocument(qmp).toJson());

            args["up"] = true;
            qmp["arguments"] = args;

            vm_process->write(QJsonDocument(qmp).toJson());
        },
        Qt::QueuedConnection);
}

mp::QemuVirtualMachine::~QemuVirtualMachine()
{
    if (vm_process)
    {
        update_shutdown_status = false;

        if (state == State::running)
        {
            suspend();
        }
        else
        {
            shutdown();
        }
    }
}

void mp::QemuVirtualMachine::start()
{
    initialize_vm_process();

    if (state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Resuming from a suspended state"));

        update_shutdown_status = true;
        is_starting_from_suspend = true;
        network_deadline = std::chrono::steady_clock::now() + 5s;
    }
    else
    {
        monitor->update_metadata_for(
            vm_name, generate_metadata(qemu_platform->vmstate_platform_args(), vm_process->arguments()));
    }

    vm_process->start();

    if (!vm_process->wait_for_started())
    {
        auto process_state = vm_process->process_state();
        if (process_state.error)
        {
            mpl::log(mpl::Level::error, vm_name, fmt::format("Qemu failed to start: {}", process_state.error->message));
            throw std::runtime_error(fmt::format("failed to start qemu instance: {}", process_state.error->message));
        }
        else if (process_state.exit_code)
        {
            mpl::log(mpl::Level::error, vm_name,
                     fmt::format("Qemu quit unexpectedly with exit code {} and with output:\n{}",
                                 process_state.exit_code.value(), vm_process->read_all_standard_error()));
            throw std::runtime_error(
                fmt::format("qemu quit unexpectedly with exit code {}, check logs for more details",
                            process_state.exit_code.value()));
        }
    }

    vm_process->write(qmp_execute_json("qmp_capabilities"));
}

void mp::QemuVirtualMachine::stop()
{
    shutdown();
}

void mp::QemuVirtualMachine::shutdown()
{
    if (state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring shutdown issued while suspended"));
    }
    else if ((state == State::running || state == State::delayed_shutdown || state == State::unknown) &&
             vm_process->running())
    {
        vm_process->write(qmp_execute_json("system_powerdown"));
        vm_process->wait_for_finished();
    }
    else
    {
        if (state == State::starting)
            update_shutdown_status = false;

        if (vm_process)
        {
            vm_process->kill();
        }
    }
}

void mp::QemuVirtualMachine::suspend()
{
    if ((state == State::running || state == State::delayed_shutdown) && vm_process->running())
    {
        if (update_shutdown_status)
        {
            state = State::suspending;
            update_state();
            update_shutdown_status = false;
        }

        vm_process->write(hmc_to_qmp_json("savevm " + QString::fromStdString(suspend_tag)));
        vm_process->wait_for_finished();
        vm_process.reset(nullptr);
    }
    else if (state == State::off || state == State::suspended)
    {
        mpl::log(mpl::Level::info, vm_name, fmt::format("Ignoring suspend issued while stopped/suspended"));
        monitor->on_suspend();
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

void mp::QemuVirtualMachine::update_state()
{
    monitor->persist_state_for(vm_name, state);
}

void mp::QemuVirtualMachine::on_started()
{
    state = VirtualMachine::State::starting;
    update_state();
    monitor->on_resume();
}

void mp::QemuVirtualMachine::on_error()
{
    state = State::off;
    update_state();
}

void mp::QemuVirtualMachine::on_shutdown()
{
    std::unique_lock<decltype(state_mutex)> lock{state_mutex};
    auto current_state = state;

    state = State::off;
    if (current_state == State::starting)
    {
        if (!saved_error_msg.empty() && saved_error_msg.back() != '\n')
            saved_error_msg.append("\n");
        saved_error_msg.append(fmt::format("{}: shutdown called while starting", vm_name));
        state_wait.wait(lock, [this] { return shutdown_while_starting; });
    }

    management_ip = std::nullopt;
    update_state();
    vm_process.reset(nullptr);
    lock.unlock();
    monitor->on_shutdown();
}

void mp::QemuVirtualMachine::on_suspend()
{
    state = State::suspended;
    monitor->on_suspend();
}

void mp::QemuVirtualMachine::on_restart()
{
    qemu_platform->release_mac_with_different_hostname(mac_addr, vm_name);

    state = State::restarting;
    update_state();

    management_ip = std::nullopt;

    monitor->on_restart(vm_name);
}

void mp::QemuVirtualMachine::ensure_vm_is_running()
{
    if (is_starting_from_suspend)
    {
        // Due to https://github.com/canonical/multipass/issues/2374, the DHCP address is removed from
        // the dnsmasq leases file, so if the daemon restarts while an instance is suspended and then
        // starts the instance, the daemon won't be able to reach the instance since the instance
        // won't refresh it's IP address.  The following will force the instance to refresh by resetting
        // the network at 5 seconds and then every 30 seconds until the start timeout is reached.
        if (std::chrono::steady_clock::now() > network_deadline)
        {
            network_deadline = std::chrono::steady_clock::now() + 30s;
            emit on_reset_network();
        }
    }

    auto is_vm_running = [this] { return (vm_process && vm_process->running()); };

    mp::backend::ensure_vm_is_running_for(this, is_vm_running, saved_error_msg);
}

std::string mp::QemuVirtualMachine::ssh_hostname(std::chrono::milliseconds timeout)
{
    auto get_ip = [this]() -> std::optional<IPAddress> { return qemu_platform->get_ip_for(mac_addr); };

    return mp::backend::ip_address_for(this, get_ip, timeout);
}

std::string mp::QemuVirtualMachine::ssh_username()
{
    return username;
}

std::string mp::QemuVirtualMachine::management_ipv4()
{
    if (!management_ip)
    {
        auto result = qemu_platform->get_ip_for(mac_addr);
        if (result)
            management_ip.emplace(result.value());
        else
            return "UNKNOWN";
    }

    return management_ip.value().as_string();
}

std::string mp::QemuVirtualMachine::ipv6()
{
    return {};
}

void mp::QemuVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    mp::utils::wait_until_ssh_up(this, timeout, std::bind(&QemuVirtualMachine::ensure_vm_is_running, this));

    if (is_starting_from_suspend)
    {
        emit on_delete_memory_snapshot();
    }
}

void mp::QemuVirtualMachine::initialize_vm_process()
{
    vm_process = make_qemu_process(
        desc,
        ((state == State::suspended) ? std::make_optional(monitor->retrieve_metadata_for(vm_name)) : std::nullopt),
        mount_args, qemu_platform->vm_platform_args(desc));

    QObject::connect(vm_process.get(), &Process::started, [this]() {
        mpl::log(mpl::Level::info, vm_name, "process started");
        on_started();
    });

    QObject::connect(vm_process.get(), &Process::ready_read_standard_output, [this]() {
        auto qmp_output = vm_process->read_all_standard_output();
        mpl::log(mpl::Level::debug, vm_name, fmt::format("QMP: {}", qmp_output));
        auto qmp_object = QJsonDocument::fromJson(qmp_output.split('\n').first()).object();
        auto event = qmp_object["event"];

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
            else if (event.toString() == "STOP")
            {
                mpl::log(mpl::Level::info, vm_name, "VM suspending");
            }
            else if (event.toString() == "RESUME")
            {
                mpl::log(mpl::Level::info, vm_name, "VM suspended");
                if (state == State::suspending || state == State::running)
                {
                    vm_process->kill();
                    on_suspend();
                }
            }
        }
    });

    QObject::connect(vm_process.get(), &Process::ready_read_standard_error, [this]() {
        saved_error_msg = vm_process->read_all_standard_error().data();
        mpl::log(mpl::Level::warning, vm_name, saved_error_msg);
    });

    QObject::connect(vm_process.get(), &Process::state_changed, [this](QProcess::ProcessState newState) {
        mpl::log(mpl::Level::info, vm_name,
                 fmt::format("process state changed to {}", utils::qenum_to_string(newState)));
    });

    QObject::connect(
        vm_process.get(), &Process::error_occurred, [this](QProcess::ProcessError error, QString error_string) {
            // We just kill the process when suspending, so we don't want to print
            // out any scary error messages for this state
            if (update_shutdown_status)
            {
                mpl::log(mpl::Level::error, vm_name,
                         fmt::format("process error occurred {} {}", utils::qenum_to_string(error), error_string));
                on_error();
            }
        });

    QObject::connect(vm_process.get(), &Process::finished, [this](ProcessState process_state) {
        if (process_state.exit_code)
        {
            mpl::log(mpl::Level::info, vm_name,
                     fmt::format("process finished with exit code {}", process_state.exit_code.value()));
        }
        if (process_state.error)
        {
            if (process_state.error->state == QProcess::Crashed &&
                (state == State::suspending || state == State::suspended))
            {
                // when suspending, we ask Qemu to savevm. Once it confirms that's done, we kill it. Catch the "crash"
                mpl::log(mpl::Level::debug, vm_name, "Suspended VM successfully stopped");
            }
            else
            {
                mpl::log(mpl::Level::error, vm_name, fmt::format("error: {}", process_state.error->message));
            }
        }

        if (update_shutdown_status || state == State::starting)
        {
            on_shutdown();
        }
    });
}

void mp::QemuVirtualMachine::update_cpus(int num_cores)
{
    assert(num_cores > 0);
    desc.num_cores = num_cores;
}

void mp::QemuVirtualMachine::resize_memory(const MemorySize& new_size)
{
    desc.mem_size = new_size;
}

void mp::QemuVirtualMachine::resize_disk(const MemorySize& new_size)
{
    assert(new_size > desc.disk_space);

    mp::backend::resize_instance_image(new_size, desc.image.image_path);
    desc.disk_space = new_size;
}

mp::MountHandler::UPtr mp::QemuVirtualMachine::make_native_mount_handler(const SSHKeyProvider* ssh_key_provider,
                                                                         const std::string& target,
                                                                         const VMMount& mount)
{
    return std::make_unique<QemuMountHandler>(this, ssh_key_provider, target, mount);
}

mp::QemuVirtualMachine::MountArgs& mp::QemuVirtualMachine::modifiable_mount_args()
{
    return mount_args;
}
