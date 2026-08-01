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

#include "sshfs_client_composer.h"

#include <multipass/format.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>

namespace mp = multipass;

namespace
{
constexpr auto base_options = "-o slave -o transform_symlinks -o allow_other -o Compression=no";

std::string find_sshfs(mp::SSHSession& session)
{
    return MP_UTILS.run_in_ssh_session(session, "sudo which sshfs");
}
} // namespace

std::string mp::SshfsClientComposer::compose_client_command(SSHSession& session,
                                                            const std::string& source,
                                                            const std::string& target) const
{
    const auto sshfs_exec_line = fmt::format("{} {}", find_sshfs(session), base_options);

    return fmt::format("sudo -n {} :{:?} {:?}", sshfs_exec_line, source, target);
}
