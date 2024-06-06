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

#include "sshfs_server_process_spec.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/id_mappings.h>
#include <multipass/logging/log.h>
#include <multipass/snap_utils.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
QString serialise_id_mappings(const mp::id_mappings& xid_mappings)
{
    QString out;
    for (auto ids : xid_mappings)
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
                         << QString::fromStdString(config.target_path) << serialise_id_mappings(config.uid_mappings)
                         << serialise_id_mappings(config.gid_mappings)
                         << QString::number(static_cast<int>(mp::logging::get_logging_level()));
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

    # CLASSIC ONLY: need to specify required libs from core snap
    /{,var/lib/snapd/}snap/core18/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

    # allow full access just to this user-specified source directory on the host
    %4/ rw,
    %4/** rwlk,
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir; // sshfs_server is a multipass utility, is located relative to the multipassd binary if not in a
                      // snap. If snapped, is located relative to $SNAP
    QString signal_peer; // if snap confined, specify only multipassd can kill dnsmasq

    try
    {
        root_dir = mpu::snap_dir();
        signal_peer = "snap.multipass.multipassd";
    }
    catch (const mp::SnapEnvironmentException&)
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
