/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/sshfs_mount/sshfs_mounts.h>
#include <multipass/sshfs_server_config.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <QEventLoop>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs-mounts";

template <typename Signal>
void start_and_block_until(mp::Process* process, Signal signal, std::function<bool(mp::Process* process)> ready_decider)
{
    QEventLoop event_loop;
    auto stop_conn =
        QObject::connect(process, &mp::Process::finished, [&event_loop](mp::ProcessState) { event_loop.quit(); });

    auto running_conn = QObject::connect(process, signal, [process, ready_decider, &event_loop]() {
        if (ready_decider(process))
        {
            event_loop.quit();
        }
    });

    process->start();

    // This blocks and waits either ready_decider to return true, or failure.
    event_loop.exec();

    QObject::disconnect(stop_conn);
    QObject::disconnect(running_conn);
}
} // namespace

mp::SSHFSMounts::SSHFSMounts(const SSHKeyProvider& key_provider) : key(key_provider.private_key_as_base64())
{
}

void mp::SSHFSMounts::start_mount(VirtualMachine* vm, const std::string& source_path, const std::string& target_path,
                                  const std::unordered_map<int, int>& gid_map,
                                  const std::unordered_map<int, int>& uid_map)
{
    mp::SSHFSServerConfig config;
    config.host = vm->ssh_hostname();
    config.port = vm->ssh_port();
    config.username = vm->ssh_username();
    config.instance = vm->vm_name;
    config.target_path = target_path;
    config.source_path = source_path;
    config.uid_map = uid_map;
    config.gid_map = gid_map;
    config.private_key = key;

    auto sshfs_server_process_t = mp::platform::make_sshfs_server_process(config);
    // FIXME: ProcessFactory really should return qt_delete_later_unique_ptr<Process> as Process emits signals
    // and the respective slots may be called on the event loop, but unique_ptr can delete the Process before
    // the slots are fired, causing a crash.
    mp::qt_delete_later_unique_ptr<mp::Process> sshfs_server_process(sshfs_server_process_t.release());

    QObject::connect(
        sshfs_server_process.get(), &mp::Process::finished, this,
        [this, instance = vm->vm_name, target_path](mp::ProcessState exit_state) {
            if (exit_state.completed_successfully())
            {
                mpl::log(mpl::Level::info, category,
                         fmt::format("Mount '{}' in instance \"{}\" has stopped", target_path, instance));
            }
            else
            {
                mpl::log(mpl::Level::warning, // not error as it failing can indicate we need to install sshfs in the VM
                         category,
                         fmt::format("Mount '{}' in instance \"{}\" has stopped unexpectedly: {}", target_path,
                                     instance, exit_state.failure_message()));
            }

            mount_processes[instance].erase(target_path);
        });

    QObject::connect(
        sshfs_server_process.get(), &mp::Process::error_occurred, this,
        [instance = vm->vm_name, target_path, process = sshfs_server_process.get()](QProcess::ProcessError error,
                                                                                    QString error_string) {
            mpl::log(mpl::Level::error, category,
                     fmt::format("There was an error with sshfs_server for instance \"{}\" with path '{}': {} - {}",
                                 instance, target_path, mp::utils::qenum_to_string(error), error_string));
        });

    mpl::log(mpl::Level::info, category, fmt::format("mounting {} => {} in {}", source_path, target_path, vm->vm_name));
    mpl::log(mpl::Level::info, category,
             fmt::format("process program '{}'", sshfs_server_process->program().toStdString()));
    mpl::log(mpl::Level::info, category,
             fmt::format("process arguments '{}'", sshfs_server_process->arguments().join(", ").toStdString()));

    start_and_block_until(
        sshfs_server_process.get(), &mp::Process::ready_read_standard_output, [](mp::Process* process) {
            return process->read_all_standard_output().contains("Connected"); // Magic string printed by sshfs_server
        });

    // Check in case sshfs_server stopped, usually due to an error
    auto process_state = sshfs_server_process->process_state();
    if (process_state.exit_code == 9) // Magic number returned by sshfs_server
    {
        throw mp::SSHFSMissingError();
    }
    else if (process_state.exit_code || process_state.error)
    {
        throw std::runtime_error(
            fmt::format("{}: {}", process_state.failure_message(), sshfs_server_process->read_all_standard_error()));
    }

    mount_processes[vm->vm_name][target_path] = std::move(sshfs_server_process);
}

bool mp::SSHFSMounts::stop_mount(const std::string& instance, const std::string& path)
{
    auto sshfs_mount_it = mount_processes.find(instance);
    if (sshfs_mount_it == mount_processes.end())
    {
        return false;
    }

    auto& sshfs_mount_map = sshfs_mount_it->second;
    auto map_entry = sshfs_mount_map.find(path);
    if (map_entry != sshfs_mount_map.end())
    {
        auto& sshfs_mount = map_entry->second;
        mpl::log(mpl::Level::info, category,
                 fmt::format("stopping sshfs_server for \"{}\" serving '{}'", instance, path));
        sshfs_mount->terminate(); // TODO - if non-responsive, then kill()
        return true;
    }
    return false;
}

void mp::SSHFSMounts::stop_all_mounts_for_instance(const std::string& instance)
{
    auto mounts_it = mount_processes.find(instance);
    if (mounts_it == mount_processes.end() || mounts_it->second.empty())
    {
        mpl::log(mpl::Level::debug, category, fmt::format("No mounts to stop for instance \"{}\"", instance));
    }
    else
    {
        for (auto& sshfs_mount : mounts_it->second)
        {
            mpl::log(mpl::Level::debug, category,
                     fmt::format("Stopping mount '{}' in instance \"{}\"", sshfs_mount.first, instance));
            sshfs_mount.second->terminate();
        }
    }
    mount_processes.clear();
}

bool mp::SSHFSMounts::has_instance_already_mounted(const std::string& instance, const std::string& path) const
{
    auto entry = mount_processes.find(instance);
    if (entry != mount_processes.end() && entry->second.find(path) != entry->second.end())
    {
        return true;
    }
    return false;
}
