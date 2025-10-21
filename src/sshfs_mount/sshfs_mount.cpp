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

#include "sshfs_mount.h"
#include "sftp_server.h"

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>
#include <multipass/id_mappings.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>

#include <multipass/utils/semver_compare.h>

#include <QDir>
#include <QString>
#include <iostream>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "sshfs mount";
const std::string fuse_version_string{"FUSE library version"};
const std::string ld_library_path_key{"LD_LIBRARY_PATH="};
const std::string snap_path_key{"SNAP="};

auto get_sshfs_exec_and_options(mp::SSHSession& session)
{
    std::string sshfs_exec;

    try
    {
        // Prefer to use Snap package version first
        auto sshfs_env{MP_UTILS.run_in_ssh_session(session, "snap run multipass-sshfs.env")};

        auto ld_library_path = mp::utils::match_line_for(sshfs_env, ld_library_path_key);
        auto snap_path = mp::utils::match_line_for(sshfs_env, snap_path_key);
        snap_path = snap_path.substr(snap_path_key.length());

        sshfs_exec = fmt::format("env {} {}/bin/sshfs", ld_library_path, snap_path);
    }
    catch (const std::exception& e)
    {
        mpl::debug(category, "'multipass-sshfs' snap package is not installed: {}", e.what());

        // Fallback to looking for distro version if snap is not found
        try
        {
            sshfs_exec = MP_UTILS.run_in_ssh_session(session, "sudo which sshfs");
        }
        catch (const std::exception& e)
        {
            mpl::warn(category, "Unable to determine if 'sshfs' is installed: {}", e.what());
            throw mp::SSHFSMissingError();
        }
    }

    sshfs_exec = mp::utils::trim_end(sshfs_exec);

    auto version_info{MP_UTILS.run_in_ssh_session(session, fmt::format("sudo {} -V", sshfs_exec))};

    sshfs_exec += " -o slave -o transform_symlinks -o allow_other -o Compression=no";

    auto fuse_version_line = mp::utils::match_line_for(version_info, fuse_version_string);
    if (!fuse_version_line.empty())
    {
        using namespace multipass::literals;
        std::string fuse_version_str;

        // split on the fuse_version_string along with 0 or 1 colon(s)
        auto tokens =
            mp::utils::split(fuse_version_line, fmt::format("{}:? ", fuse_version_string));
        if (tokens.size() == 2)
            fuse_version_str = tokens[1];

        if (fuse_version_str.empty())
        {
            mpl::warn(category, "Unable to parse the {}", fuse_version_string);
            mpl::debug(category,
                       "Unable to parse the {}: {}",
                       fuse_version_string,
                       fuse_version_line);
        }
        // The option was made the default in libfuse 3.0
        else if (multipass::opaque_semver{fuse_version_str} < "3.0.0"_semver)
        {
            sshfs_exec += " -o nonempty -o cache=no";
        }
        else
        {
            sshfs_exec += " -o dir_cache=no";
        }
    }
    else
    {
        mpl::warn(category, "Unable to retrieve \'{}\'", fuse_version_string);
    }

    return sshfs_exec;
}

auto make_sftp_server(mp::SSHSession&& session,
                      const std::string& source,
                      const std::string& target,
                      const mp::id_mappings& gid_mappings,
                      const mp::id_mappings& uid_mappings)
{
    mpl::debug(category,
               "{}:{} {}(source = {}, target = {}, …): ",
               __FILE__,
               __LINE__,
               __FUNCTION__,
               source,
               target);

    auto sshfs_exec_line = get_sshfs_exec_and_options(session);

    // Split the path in existing and missing parts.
    const auto& [leading, missing] = mpu::get_path_split(session, target);

    auto output = MP_UTILS.run_in_ssh_session(session, "id -u");
    mpl::debug(category, "{}:{} {}(): `id -u` = {}", __FILE__, __LINE__, __FUNCTION__, output);
    auto default_uid = std::stoi(output);

    output = MP_UTILS.run_in_ssh_session(session, "id -g");
    mpl::debug(category, "{}:{} {}(): `id -g` = {}", __FILE__, __LINE__, __FUNCTION__, output);
    auto default_gid = std::stoi(output);

    // We need to create the part of the path which does not still exist,
    // and set then the correct ownership.
    if (missing != ".")
    {
        mpu::make_target_dir(session, leading, missing);
        mpu::set_owner_for(session, leading, missing, default_uid, default_gid);
    }

    return std::make_unique<mp::SftpServer>(std::move(session),
                                            source,
                                            leading + missing,
                                            gid_mappings,
                                            uid_mappings,
                                            default_uid,
                                            default_gid,
                                            sshfs_exec_line);
}

} // namespace

mp::SshfsMount::SshfsMount(SSHSession&& session,
                           const std::string& source,
                           const std::string& target,
                           const mp::id_mappings& gid_mappings,
                           const mp::id_mappings& uid_mappings)
    : sftp_server{make_sftp_server(std::move(session), source, target, gid_mappings, uid_mappings)},
      sftp_thread{[this] {
          state.store(State::Running, std::memory_order_release);

          mp::top_catch_all(category, [this] {
              std::cout << "Connected" << std::endl;
              sftp_server->run();
              std::cout << "Stopped" << std::endl;
          });

          state.store(State::Stopped, std::memory_order_release);
      }}
{
}

mp::SshfsMount::~SshfsMount()
{
    state.store(State::Stopped, std::memory_order_release);
    stop();
}

void mp::SshfsMount::stop()
{
    sftp_server->stop();
    if (sftp_thread.joinable())
        sftp_thread.join();
}

bool mp::SshfsMount::alive() const
{
    return state.load(std::memory_order_acquire) != State::Stopped;
}
