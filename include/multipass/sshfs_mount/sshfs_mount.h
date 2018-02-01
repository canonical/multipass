/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#ifndef MULTIPASS_SSHFS_MOUNT
#define MULTIPASS_SSHFS_MOUNT

#include <multipass/ssh/ssh_session.h>
#include <multipass/virtual_machine.h>

#include <functional>
#include <iostream>
#include <thread>
#include <unordered_map>

#include <QObject>
#include <QString>

namespace multipass
{
class SshfsMount : public QObject
{
    Q_OBJECT

public:
    SshfsMount(std::function<std::unique_ptr<SSHSession>()> session_factory, const QString& source,
               const QString& target, const std::unordered_map<int, int>& gid_map,
               const std::unordered_map<int, int>& uid_map, std::ostream& cout);
    virtual ~SshfsMount();

    void run();
    void stop();

signals:
    void finished();

private:
    std::function<std::unique_ptr<SSHSession>()> session_factory;
    std::unique_ptr<SSHSession> ssh_session;
    SSHProcess sshfs_process;
    const QString sshfs_pid;
    const std::unordered_map<int, int> gid_map;
    const std::unordered_map<int, int> uid_map;
    std::thread mount_thread;
    std::ostream& cout;
};
}
#endif // MULTIPASS_SSHFS_MOUNT
