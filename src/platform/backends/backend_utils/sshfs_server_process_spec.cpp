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
#include <QCryptographicHash>
#include <QDir>

#include <multipass/snap_utils.h>

namespace mp = multipass;
namespace mu = multipass::utils;

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

QByteArray gen_hash(const std::string& path)
{
    // need to return unique name for each mount.  The target directory string will be unique,
    // so hash it and return first 8 hex chars.
    return QCryptographicHash::hash(QByteArray::fromStdString(path), QCryptographicHash::Sha256).toHex().left(8);
}

} // namespace

mp::SSHFSServerProcessSpec::SSHFSServerProcessSpec(const SSHFSServerConfig& config)
    : config(config), target_hash(gen_hash(config.source_path))
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
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>
    #include <abstractions/nameservice>

    # Sshfs_server requires broad filesystem altering permissions, but only for the
    # host directory the user has specified to be shared with the VM.

    # Required for reading and searching host directories
    capability dac_override,
    capability dac_read_search,
    # Enables modifying of file ownership and permissions
    capability chown,
    capability fsetid,
    capability fowner,
    # Multipass allows user to specify arbitrary uid/gid mappings
    capability setuid,
    capability setgid,

    # Allow multipassd send sshfs_server signals
    signal (receive) peer=%2,

    # sshfs gathers some info about system resources
    /sys/devices/system/node/ r,
    /sys/devices/system/node/node[0-9]*/meminfo r,

    # binary and its libs
    %3/bin/sshfs_server ixr,
    %3/{usr/,}lib/** rm,

    # allow full access just to this user-specified source directory on the host
    %4/ rw,
    %4/** rwlk,
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir; // sshfs_server is a multipass utility, is located relative to the multipassd binary if not in a
                      // snap. If snapped, is located relative to $SNAP
    QString signal_peer; // if snap confined, specify only multipassd can kill dnsmasq

    if (mu::is_snap_confined())
    {
        root_dir = mu::snap_dir();
        signal_peer = "snap.multipass.multipassd";
    }
    else
    {
        QDir application_dir(QCoreApplication::applicationDirPath());
        application_dir.cdUp();
        root_dir = application_dir.absolutePath();
        signal_peer = "unconfined";
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, root_dir,
                                QString::fromStdString(config.source_path));
}

QString mp::SSHFSServerProcessSpec::identifier() const
{
    return QString::fromStdString(config.instance) + "." + target_hash;
}

void mp::SSHFSServerProcessSpec::setup_child_process() const
{
    // Inform kernel to send sigquit this child when its parent process (multipassd) dies unexpectedly
    // IMPORTANT NOTE: if child calls setuid/gid, this prctl state is cleared by the kernel. So this will
    // not work for dnsmasq for instance, but does work for sshfs_server.

    // EVEN IMPORTANT NOTE: the libapparmor call aa_change_onexec resets this on exec, so this code
    // does not work with AppArmor. To fix, beed a way to re-set this after apparmor. "setpriv" from sys-utils
    // can, should copy how it does it..
    // const int r = prctl(PR_SET_PDEATHSIG, SIGQUIT);
    // if (r == -1)
    // {
    //     perror(0);
    //     exit(1);
    // }
    // // test in case the original parent exited just before the prctl() call
    // if (getppid() != ::daemon_pid)
    //     exit(1);
}
