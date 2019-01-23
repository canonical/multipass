/*
 * Copyright (C) 2018 Canonical, Ltd.
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

QString mp::SSHFSServerProcessSpec::apparmor_profile() const
{
    // Profile based on https://github.com/Rafiot/apparmor-profiles/blob/master/profiles/usr.sbin.dnsmasq
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>
    #include <abstractions/dbus>
    #include <abstractions/nameservice>

    capability chown,
    capability net_bind_service,
    capability setgid,
    capability setuid,
    capability dac_override,
    capability net_admin,         # for DHCP server
    capability net_raw,           # for DHCP server ping checks
    network inet raw,
    network inet6 raw,
}
    )END");

    // If running as a snap, presuming fully confined, so need to add rule to allow mmap of binary to be launched.
    const QString snap_dir = qgetenv("SNAP"); // validate??
    QString signal_peer;

    if (!snap_dir.isEmpty()) // if snap confined, specify only multipassd can kill dnsmasq
    {
        signal_peer = "peer=snap.multipass.multipassd";
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, snap_dir);
}

QString multipass::SSHFSServerProcessSpec::identifier() const
{
    return QString::fromStdString(config.instance);
}
