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

#ifndef MULTIPASS_SSHFSMOUNTS_H
#define MULTIPASS_SSHFSMOUNTS_H

#include <QProcess>
#include <memory>
#include <string>
#include <unordered_map>

#include <multipass/virtual_machine.h>

namespace multipass
{

class SSHFSMounts : public QObject
{
    Q_OBJECT
public:
    explicit SSHFSMounts(const SSHKeyProvider& ssh_key_provider);

    void start_mount(const VirtualMachine::UPtr& vm, const std::string& instance, const std::string& source_path,
                     const std::string& target_path, const std::unordered_map<int, int>& gid_map,
                     const std::unordered_map<int, int>& uid_map);

    bool stop_mount(const std::string& instance, const std::string& path);
    void stop_all_mounts_for_instance(const std::string& instance);

    bool has_instance_already_mounted(const std::string& instance, const std::string& path) const;

private:
    const std::string key;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<QProcess>>> mount_processes;
};

} // namespace multipass
#endif // MULTIPASS_SSHFSMOUNTS_H
