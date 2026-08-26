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

#include "sshfs_client_steward.h"

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/utils/semver_compare.h>

#include <exception>
#include <stdexcept>
#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs steward";
constexpr auto base_options = "-o slave -o transform_symlinks -o allow_other -o Compression=no";
const std::string fuse_version_string{"FUSE library version"};
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
        return MP_UTILS.run_in_ssh_session(session, "sudo -n which sshfs");
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "Unable to determine if 'sshfs' is installed: {}", e.what());
        throw mp::SSHFSMissingError{};
    }
}

std::string fuse_options_for(mp::SSHSession& session, const std::string& sshfs)
{
    const auto version_info = MP_UTILS.run_in_ssh_session(session,
                                                          fmt::format("sudo -n {} -V", sshfs));
    const auto fuse_version_line = mp::utils::match_line_for(version_info, fuse_version_string);

    if (fuse_version_line.empty())
    {
        mpl::warn(category, "Unable to retrieve \'{}\'", fuse_version_string);
        return {};
    }

    // Split on the version string, along with 0 or 1 colon(s)
    const auto tokens = mp::utils::split(fuse_version_line,
                                         fmt::format("{}:? ", fuse_version_string));

    try
    {
        if (tokens.size() != 2)
            throw std::invalid_argument{fuse_version_line};

        using namespace multipass::literals;

        // libfuse 3.0 renamed {,dir_}cache and removed nonempty (which became the default behavior)
        return mp::opaque_semver{tokens[1]} < "3.0.0"_semver ? " -o nonempty -o cache=no"
                                                             : " -o dir_cache=no";
    }
    catch (const std::invalid_argument& e)
    {
        mpl::warn(category, "Unable to parse the {}", fuse_version_string);
        mpl::debug(category, "Unable to parse the {}: {}", fuse_version_string, e.what());
        return {};
    }
}
} // namespace

std::string mp::SshfsClientSteward::compose_client_command(SSHSession& session,
                                                           const std::string& source,
                                                           const std::string& target) const
{
    const auto sshfs = find_sshfs(session);
    const auto sshfs_exec_line = fmt::format("{} {}{}",
                                             sshfs,
                                             base_options,
                                             fuse_options_for(session, sshfs));

    return fmt::format("sudo -n {} :{:?} {:?}", sshfs_exec_line, source, target);
}

void mp::SshfsClientSteward::clean_up_after_client(SSHSession& session,
                                                   const std::string& source) const
{
    const auto mount_path = [&session, &source] {
        auto proc = session.exec(fmt::format("findmnt --source :{:?} -o TARGET -n", source));
        return mp::utils::trim(proc->read_std_output());
    }();

    if (!mount_path.empty())
        MP_UTILS.run_in_ssh_session(session, fmt::format("sudo -n umount {:?}", mount_path));
}
