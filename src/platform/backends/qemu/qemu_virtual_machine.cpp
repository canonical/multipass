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

#include "qemu_virtual_machine.h"
#include "qemu_mount_handler.h"
#include "qemu_snapshot.h"
#include "qemu_vm_process_spec.h"
#include "qemu_vmstate_process_spec.h"

#include <shared/qemu_img_utils/qemu_img_utils.h>
#include <shared/shared_backend_utils.h>

#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/vm_mount.h>
#include <multipass/vm_status_monitor.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QTemporaryFile>

#include <cassert>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace std::chrono_literals;

namespace
{
constexpr auto suspend_tag = "suspend";
constexpr auto machine_type_key = "machine_type";
constexpr auto arguments_key = "arguments";
constexpr auto mount_data_key = "mount_data";
constexpr auto mount_source_key = "source";
constexpr auto mount_arguments_key = "arguments";

constexpr int shutdown_timeout = 300000;   // unit: ms, 5 minute timeout for shutdown/suspend
constexpr int kill_process_timeout = 5000; // unit: ms, 5 seconds timeout for killing the process

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

auto mount_args_from_json(const QJsonObject& object)
{
    mp::QemuVirtualMachine::MountArgs mount_args;
    auto mount_data_map = object[mount_data_key].toObject();
    for (const auto& tag : mount_data_map.keys())
    {
        const auto mount_data = mount_data_map[tag].toObject();
        const auto source = mount_data[mount_source_key];
        auto args = mount_data[mount_arguments_key].toArray();
        if (!source.isString() || !std::all_of(args.begin(), args.end(), std::mem_fn(&QJsonValueRef::isString)))
            continue;
        mount_args[tag.toStdString()] = {source.toString().toStdString(),
                                         QVariant{args.toVariantList()}.toStringList()};
    }
    return mount_args;
}

auto make_qemu_process(const mp::VirtualMachineDescription& desc, const std::optional<QJsonObject>& resume_metadata,
                       const mp::QemuVirtualMachine::MountArgs& mount_args, const QStringList& platform_args)
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

auto mount_args_to_json(const mp::QemuVirtualMachine::MountArgs& mount_args)
{
    QJsonObject object;
    for (const auto& [tag, mount_data] : mount_args)
    {
        const auto& [source, args] = mount_data;
        object[QString::fromStdString(tag)] = QJsonObject{{mount_source_key, QString::fromStdString(source)},
                                                          {mount_arguments_key, QJsonArray::fromStringList(args)}};
    }
    return object;
}

auto generate_metadata(const QStringList& platform_args, const QStringList& proc_args,
                       const mp::QemuVirtualMachine::MountArgs& mount_args)
{
    QJsonObject metadata;
    metadata[machine_type_key] = get_qemu_machine_type(platform_args);
    metadata[arguments_key] = QJsonArray::fromStringList(proc_args);
    metadata[mount_data_key] = mount_args_to_json(mount_args);
    return metadata;
}

void convert_to_qcow2_v3_if_necessary(const mp::Path& image_path, const std::string& vm_name)
{
    try
    {
        // convert existing VMs to v3 too (doesn't affect images that are already v3)
        mp::backend::amend_to_qcow2_v3(image_path);
    }
    catch (const mp::backend::QemuImgException& e)
    {
        mpl::log(mpl::Level::error, vm_name, e.what());
    }
}

QStringList extract_snapshot_tags(const QByteArray& snapshot_list_output_stream)
{
    QStringList lines = QString{snapshot_list_output_stream}.split('\n');
    QStringList snapshot_tags;

    // Snapshot list:
    // ID        TAG               VM SIZE                DATE     VM CLOCK     ICOUNT
    // 2         @s2                   0 B 2024-06-11 23:22:59 00:00:00.000          0
    // 3         @s3                   0 B 2024-06-12 12:30:37 00:00:00.000          0

    // The first two lines are headers
    for (int i = 2; i < lines.size(); ++i)
    {
        // Qt::SkipEmptyParts improve the robustness of the code, it can keep the result correct in the case of leading
        // and trailing spaces.
        QStringList entries = lines[i].split(QRegularExpression{R"(\s+)"}, Qt::SkipEmptyParts);

        if (entries.count() >= 2)
        {
            snapshot_tags.append(entries[1]);
        }
    }

    return snapshot_tags;
}

} // namespace

mp::QemuVirtualMachine::QemuVirtualMachine(const VirtualMachineDescription& desc,
                                           QemuPlatform* qemu_platform,
                                           VMStatusMonitor& monitor,
                                           const SSHKeyProvider& key_provider,
                                           const Path& instance_dir,
                                           bool remove_snapshots)
    : BaseVirtualMachine{mp::backend::instance_image_has_snapshot(desc.image.image_path, suspend_tag) ? State::suspended
                                                                                                      : State::off,
                         desc.vm_name,
                         key_provider,
                         instance_dir},
      desc{desc},
      qemu_platform{qemu_platform},
      monitor{&monitor},
      mount_args{mount_args_from_json(monitor.retrieve_metadata_for(vm_name))}
{
    convert_to_qcow2_v3_if_necessary(desc.image.image_path,
                                     vm_name); // TODO drop in a couple of releases (went in on v1.13)

    connect_vm_signals();

    // only for clone case where the vm recreation purges the snapshot data
    if (remove_snapshots)
    {
        remove_snapshots_from_backend();
    }
}

mp::QemuVirtualMachine::~QemuVirtualMachine()
{
    if (vm_process)
    {
        update_shutdown_status = false;

        mp::top_catch_all(vm_name, [this]() {
            if (state == State::running)
            {
                suspend();
            }
            else
            {
                shutdown();
            }
        });
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
        // remove the mount arguments from the rest of the arguments, as they are stored separately for easier retrieval
        auto proc_args = vm_process->arguments();
        for (const auto& [_, mount_data] : mount_args)
            for (const auto& arg : mount_data.second)
                proc_args.removeOne(arg);

        monitor->update_metadata_for(vm_name,
                                     generate_metadata(qemu_platform->vmstate_platform_args(), proc_args, mount_args));
    }

    vm_process->start();
    connect_vm_signals();

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

void mp::QemuVirtualMachine::shutdown(ShutdownPolicy shutdown_policy)
{
    std::unique_lock<std::mutex> lock{state_mutex};
    disconnect_vm_signals();

    try
    {
        check_state_for_shutdown(shutdown_policy);
    }
    catch (const VMStateIdempotentException& e)
    {
        mpl::log(mpl::Level::info, vm_name, e.what());
        return;
    }

    if (shutdown_policy == ShutdownPolicy::Poweroff)
    {
        mpl::log(mpl::Level::info, vm_name, "Forcing shutdown");

        if (vm_process)
        {
            mpl::log(mpl::Level::info, vm_name, "Killing process");
            force_shutdown = true;
            lock.unlock();
            vm_process->kill();
            if (vm_process != nullptr && !vm_process->wait_for_finished(kill_process_timeout))
            {
                throw std::runtime_error{
                    fmt::format("The QEMU process did not finish within {} milliseconds after being killed",
                                kill_process_timeout)};
            }
        }
        else
        {
            mpl::log(mpl::Level::debug, vm_name, "No process to kill");
        }

        const auto has_suspend_snapshot = mp::backend::instance_image_has_snapshot(desc.image.image_path, suspend_tag);
        if (has_suspend_snapshot != (state == State::suspended)) // clang-format off
            mpl::log(mpl::Level::warning, vm_name, fmt::format("Image has {} suspension snapshot, but the state is {}",
                                                               has_suspend_snapshot ? "a" : "no",
                                                               static_cast<short>(state))); // clang-format on

        if (has_suspend_snapshot)
        {
            mpl::log(mpl::Level::info, vm_name, "Deleting suspend image");
            mp::backend::delete_snapshot_from_image(desc.image.image_path, suspend_tag);
        }

        state = State::off;
    }
    else
    {
        lock.unlock();

        drop_ssh_session();

        if (vm_process && vm_process->running())
        {
            vm_process->write(qmp_execute_json("system_powerdown"));
            if (vm_process->wait_for_finished(shutdown_timeout))
            {
                lock.lock();
                state = State::off;
            }
            else
            {
                throw std::runtime_error{
                    fmt::format("The QEMU process did not finish within {} milliseconds after being shutdown",
                                shutdown_timeout)};
            }
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

        drop_ssh_session();
        vm_process->write(hmc_to_qmp_json(QString{"savevm "} + suspend_tag));
        vm_process->wait_for_finished(shutdown_timeout);

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
        drop_ssh_session();
        update_state();
        vm_process.reset(nullptr);
    }

    monitor->on_shutdown();
}

void mp::QemuVirtualMachine::on_suspend()
{
    drop_ssh_session();
    state = State::suspended;
    monitor->on_suspend();
}

void mp::QemuVirtualMachine::on_restart()
{
    drop_ssh_session();
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
    auto get_ip = [this]() -> std::optional<IPAddress> { return qemu_platform->get_ip_for(desc.default_mac_address); };

    return mp::backend::ip_address_for(this, get_ip, timeout);
}

std::string mp::QemuVirtualMachine::ssh_username()
{
    return desc.ssh_username;
}

std::string mp::QemuVirtualMachine::management_ipv4()
{
    if (!management_ip)
    {
        auto result = qemu_platform->get_ip_for(desc.default_mac_address);
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
    BaseVirtualMachine::wait_until_ssh_up(timeout);

    if (is_starting_from_suspend)
    {
        emit on_delete_memory_snapshot();
        emit on_synchronize_clock();
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
                const auto log_level = force_shutdown ? mpl::Level::info : mpl::Level::error;
                mpl::log(log_level,
                         vm_name,
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
                const auto log_level = force_shutdown ? mpl::Level::info : mpl::Level::error;
                mpl::log(log_level, vm_name, fmt::format("error: {}", process_state.error->message));

                // reset force_shutdown so that subsequent errors can be accurately reported
                force_shutdown = false;
            }
        }

        if (update_shutdown_status || state == State::starting)
        {
            on_shutdown();
        }
    });
}

void mp::QemuVirtualMachine::connect_vm_signals()
{
    std::unique_lock lock{vm_signal_mutex};

    if (vm_signals_connected)
        return;

    QObject::connect(
        this,
        &QemuVirtualMachine::on_delete_memory_snapshot,
        this,
        [this] {
            mpl::log(mpl::Level::debug, vm_name, fmt::format("Deleted memory snapshot"));
            vm_process->write(hmc_to_qmp_json(QString("delvm ") + suspend_tag));
            is_starting_from_suspend = false;
        },
        Qt::QueuedConnection);

    // The following is the actual code to reset the network via QMP if an IP address is not obtained after
    // starting from suspend.  This will probably be deprecated in the future.
    QObject::connect(
        this,
        &QemuVirtualMachine::on_reset_network,
        this,
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

    QObject::connect(
        this,
        &QemuVirtualMachine::on_synchronize_clock,
        this,
        [this]() {
            try
            {
                mpl::log(mpl::Level::debug, vm_name, fmt::format("Syncing RTC clock"));
                ssh_exec("sudo timedatectl set-local-rtc 0 --adjust-system-clock");
            }
            catch (const std::exception& e)
            {
                mpl::log(mpl::Level::warning, vm_name, fmt::format("Failed to sync clock: {}", e.what()));
            }
        },
        Qt::QueuedConnection);

    vm_signals_connected = true;
}

void mp::QemuVirtualMachine::disconnect_vm_signals()
{
    std::unique_lock lock{vm_signal_mutex};

    disconnect(this, &QemuVirtualMachine::on_delete_memory_snapshot, nullptr, nullptr);
    disconnect(this, &QemuVirtualMachine::on_reset_network, nullptr, nullptr);
    disconnect(this, &QemuVirtualMachine::on_synchronize_clock, nullptr, nullptr);

    vm_signals_connected = false;
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

void mp::QemuVirtualMachine::add_network_interface(int /* not used on this backend */,
                                                   const std::string& default_mac_addr,
                                                   const NetworkInterface& extra_interface)
{
    desc.extra_interfaces.push_back(extra_interface);
    add_extra_interface_to_instance_cloud_init(default_mac_addr, extra_interface);
}

mp::MountHandler::UPtr mp::QemuVirtualMachine::make_native_mount_handler(const std::string& target,
                                                                         const VMMount& mount)
{
    return std::make_unique<QemuMountHandler>(this, &key_provider, target, mount);
}

void mp::QemuVirtualMachine::remove_snapshots_from_backend() const
{
    const QStringList snapshot_tag_list = extract_snapshot_tags(backend::snapshot_list_output(desc.image.image_path));

    for (const auto& snapshot_tag : snapshot_tag_list)
    {
        backend::delete_snapshot_from_image(desc.image.image_path, snapshot_tag);
    }
}

mp::QemuVirtualMachine::MountArgs& mp::QemuVirtualMachine::modifiable_mount_args()
{
    return mount_args;
}

auto mp::QemuVirtualMachine::make_specific_snapshot(const std::string& snapshot_name,
                                                    const std::string& comment,
                                                    const std::string& instance_id,
                                                    const VMSpecs& specs,
                                                    std::shared_ptr<Snapshot> parent) -> std::shared_ptr<Snapshot>
{
    assert(state == VirtualMachine::State::off || state == VirtualMachine::State::stopped); // would need QMP otherwise
    return std::make_shared<QemuSnapshot>(snapshot_name, comment, instance_id, std::move(parent), specs, *this, desc);
}

auto mp::QemuVirtualMachine::make_specific_snapshot(const QString& filename) -> std::shared_ptr<Snapshot>
{
    return std::make_shared<QemuSnapshot>(filename, *this, desc);
}
