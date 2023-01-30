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

#ifndef MULTIPASS_SSHFS_SERVER_PROCESS_SPEC_H
#define MULTIPASS_SSHFS_SERVER_PROCESS_SPEC_H

#include <multipass/process/process_spec.h>
#include <multipass/sshfs_server_config.h>

namespace multipass
{

class SSHFSServerProcessSpec : public ProcessSpec
{
public:
    explicit SSHFSServerProcessSpec(const SSHFSServerConfig& config);

    QString program() const override;
    QStringList arguments() const override;
    QProcessEnvironment environment() const override;

    logging::Level error_log_level() const override;

    QString apparmor_profile() const override;
    QString identifier() const override;

private:
    const SSHFSServerConfig config;
    const QByteArray target_hash;
};

} // namespace multipass

#endif // MULTIPASS_SSHFS_SERVER_PROCESS_SPEC_H
