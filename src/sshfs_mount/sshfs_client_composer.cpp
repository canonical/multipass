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

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>

#include <exception>
#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs composer";
constexpr auto base_options = "-o slave -o transform_symlinks -o allow_other -o Compression=no";
const std::string ld_library_path_key{"LD_LIBRARY_PATH="};
const std::string snap_path_key{"SNAP="};

std::string find_snap_sshfs(mp::SSHSession& session)
{
    const auto sshfs_env = MP_UTILS.run_in_ssh_session(session, "snap run multipass-sshfs.env");

    const auto ld_library_path = mp::utils::match_line_for(sshfs_env, ld_library_path_key);
    const auto snap_path = mp::utils::match_line_for(sshfs_env, snap_path_key)
                               .substr(snap_path_key.length());

    return fmt::format("env {} {}/bin/sshfs", ld_library_path, snap_path);
}

std::string find_sshfs(mp::SSHSession& session)
{
    try
    {
        return find_snap_sshfs(session); // Prefer our snap package
    }
    catch (const std::exception& e)
    {
        mpl::debug(category, "'multipass-sshfs' snap package is not installed: {}", e.what());
    }

    // Fallback to default
    try
    {
        return MP_UTILS.run_in_ssh_session(session, "sudo which sshfs");
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "Unable to determine if 'sshfs' is installed: {}", e.what());
        throw mp::SSHFSMissingError{};
    }
}
} // namespace

std::string mp::SshfsClientComposer::compose_client_command(SSHSession& session,
                                                            const std::string& source,
                                                            const std::string& target) const
{
    const auto sshfs_exec_line = fmt::format("{} {}", find_sshfs(session), base_options);

    return fmt::format("sudo -n {} :{:?} {:?}", sshfs_exec_line, source, target);
}
