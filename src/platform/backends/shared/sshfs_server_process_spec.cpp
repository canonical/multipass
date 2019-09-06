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

#include "sshfs_server_process_spec.h"

#include <QCoreApplication>

namespace mp = multipass;

namespace
{
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

mp::SSHFSServerProcessSpec::SSHFSServerProcessSpec(const SSHFSServerConfig& config) : config(config)
{
}

QString mp::SSHFSServerProcessSpec::program() const
{
    return QCoreApplication::applicationDirPath() + "/sshfs_server";
}

QStringList mp::SSHFSServerProcessSpec::arguments() const
{
    return QStringList() << QString::fromStdString(config.host) << QString::number(config.port)
                         << QString::fromStdString(config.username) << QString::fromStdString(config.source_path)
                         << QString::fromStdString(config.target_path) << serialise_id_map(config.uid_map)
                         << serialise_id_map(config.gid_map);
}

QProcessEnvironment mp::SSHFSServerProcessSpec::environment() const
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("KEY", QString::fromStdString(config.private_key));
    return env;
}

mp::logging::Level mp::SSHFSServerProcessSpec::error_log_level() const
{
    return mp::logging::Level::debug;
}

QString mp::SSHFSServerProcessSpec::apparmor_profile() const
{
    return QString();
}
