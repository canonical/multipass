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
 */

#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/sshfs_mount/sshfs_mounts.h>

#include <fmt/format.h>
#include <multipass/logging/log.h>

#include <QCoreApplication>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs-mounts";

QString serialise_id_map(const std::unordered_map<int, int>& id_map)
{
    QString out;
    for (auto ids : id_map)
    {
        out += QString("%1:%2,").arg(ids.first).arg(ids.second);
    }
    return out;
}
} // namespace

mp::SSHFSMounts::SSHFSMounts(const SSHKeyProvider& key_provider) : key(key_provider.private_key_as_base64())
{
}

void mp::SSHFSMounts::start_mount(const mp::VirtualMachine::UPtr& vm, const std::string& instance,
                                  const std::string& source_path, const std::string& target_path,
                                  const std::unordered_map<int, int>& gid_map,
                                  const std::unordered_map<int, int>& uid_map)
{
    const auto host = vm->ssh_hostname();
    const auto port = vm->ssh_port();
    const auto username = vm->ssh_username();

    auto sshfs_server_process = std::make_unique<QProcess>();
    sshfs_server_process->setProgram(QCoreApplication::applicationDirPath() + "/sshfs_server");
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("KEY", QString::fromStdString(key)); // Add an environment variable
    sshfs_server_process->setProcessEnvironment(env);
    sshfs_server_process->setArguments(
        QStringList() << QString::fromStdString(host) << QString::number(port) << QString::fromStdString(username)
                      << QString::fromStdString(source_path) << QString::fromStdString(target_path)
                      << serialise_id_map(uid_map) << serialise_id_map(gid_map));

    QObject::connect(sshfs_server_process.get(), qOverload<int>(&QProcess::finished), this,
                     [this, instance, target_path](int exitCode) {
                         mount_processes[instance].erase(target_path);
                         if (exitCode == 0)
                             mpl::log(mpl::Level::debug, category,
                                      fmt::format("Mount '{}' in instance \"{}\" has stopped", target_path, instance));
                         else
                             mpl::log(mpl::Level::debug, category,
                                      fmt::format("Mount '{}' in instance \"{}\" has stopped unexpectedly: {}",
                                                  target_path, instance));
                     },
                     Qt::QueuedConnection);

    QObject::connect(sshfs_server_process.get(), &QProcess::errorOccurred, this,
                     [instance, target_path, process = sshfs_server_process.get()](QProcess::ProcessError) {
                         mpl::log(
                             mpl::Level::debug, category,
                             fmt::format("There was an error with sshfs_server for instance \"{}\" for path '{}': {}",
                                         instance, target_path, qPrintable(process->errorString())));
                     });

    mpl::log(mpl::Level::info, category, fmt::format("mounting {} => {} in {}", source_path, target_path, instance));
    mpl::log(mpl::Level::info, category,
             fmt::format("process program '{}'", sshfs_server_process->program().toStdString()));
    mpl::log(mpl::Level::info, category,
             fmt::format("process arguments '{}'", sshfs_server_process->arguments().join(", ").toStdString()));
    sshfs_server_process->start(QIODevice::ReadOnly);

    mount_processes[instance][target_path] = std::move(sshfs_server_process);
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
        sshfs_mount->terminate(); // TODO - GERRY, should I kill() instead??
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
