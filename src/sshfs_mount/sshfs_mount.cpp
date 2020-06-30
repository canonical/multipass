/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sftp_server.h>
#include <multipass/sshfs_mount/sshfs_mount.h>
#include <multipass/utils.h>

#include <semver200.h>

#include <QDir>
#include <QString>
#include <iostream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs mount";
const std::string fuse_version_string{"FUSE library version"};
const std::string ld_library_path_key{"LD_LIBRARY_PATH="};
const std::string snap_path_key{"SNAP="};

// TODO: Need to unify all the various SSHSession::exec type of functions into
//       one place and account for reading both stdout and stderr
auto run_cmd(mp::SSHSession& session, std::string&& cmd)
{
    auto ssh_process = session.exec(cmd);
    if (ssh_process.exit_code() != 0)
        throw std::runtime_error(ssh_process.read_std_error());

    return ssh_process.read_std_output() + ssh_process.read_std_error();
}

auto get_sshfs_exec_and_options(mp::SSHSession& session)
{
    std::string sshfs_exec;

    try
    {
        // Prefer to use Snap package version first
        auto sshfs_env{run_cmd(session, "snap run multipass-sshfs.env")};

        auto ld_library_path = mp::utils::match_line_for(sshfs_env, ld_library_path_key);
        auto snap_path = mp::utils::match_line_for(sshfs_env, snap_path_key);
        snap_path = snap_path.substr(snap_path_key.length());

        sshfs_exec = fmt::format("env {} {}/bin/sshfs", ld_library_path, snap_path);
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("'multipass-sshfs' snap package is not installed: {}", e.what()));

        // Fallback to looking for distro version if snap is not found
        try
        {
            sshfs_exec = run_cmd(session, "sudo which sshfs");
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Unable to determine if 'sshfs' is installed: {}", e.what()));
            throw mp::SSHFSMissingError();
        }
    }

    sshfs_exec = mp::utils::trim_end(sshfs_exec);

    auto version_info{run_cmd(session, fmt::format("sudo {} -V", sshfs_exec))};

    sshfs_exec += " -o slave -o transform_symlinks -o allow_other";

    auto fuse_version_line = mp::utils::match_line_for(version_info, fuse_version_string);
    if (!fuse_version_line.empty())
    {
        std::string fuse_version;

        // split on the fuse_version_string along with 0 or 1 colon(s)
        auto tokens = mp::utils::split(fuse_version_line, fmt::format("{}:? ", fuse_version_string));
        if (tokens.size() == 2)
            fuse_version = tokens[1];

        if (fuse_version.empty())
        {
            mpl::log(mpl::Level::warning, category, fmt::format("Unable to parse the {}", fuse_version_string));
            mpl::log(mpl::Level::debug, category, fmt::format("Unable to parse the {}: {}", fuse_version_string, fuse_version_line));
        }
        // The option was made the default in libfuse 3.0
        else if (version::Semver200_version(fuse_version) < version::Semver200_version("3.0.0"))
        {
            sshfs_exec += " -o nonempty";
        }
    }
    else
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Unable to retrieve \'{}\'", fuse_version_string));
    }

    return sshfs_exec;
}

// Split a path into existing and to-be-created parts.
std::pair<std::string, std::string> get_path_split(mp::SSHSession& session, const std::string& target)
{
    std::string absolute;

    switch (target[0])
    {
    case '~':
        absolute =
            run_cmd(session, fmt::format("echo ~{}", mp::utils::escape_for_shell(target.substr(1, target.size() - 1))));
        mp::utils::trim_newline(absolute);
        break;
    case '/':
        absolute = target;
        break;
    default:
        absolute = run_cmd(session, fmt::format("echo $PWD/{}", mp::utils::escape_for_shell(target)));
        mp::utils::trim_newline(absolute);
        break;
    }

    std::string existing = run_cmd(
        session, fmt::format("sudo /bin/bash -c 'P=\"{}\"; while [ ! -d \"$P/\" ]; do P=\"${{P%/*}}\"; done; echo $P/'",
                             absolute));
    mp::utils::trim_newline(existing);

    return {existing,
            QDir(QString::fromStdString(existing)).relativeFilePath(QString::fromStdString(absolute)).toStdString()};
}

// Create a directory on a given root folder.
void make_target_dir(mp::SSHSession& session, const std::string& root, const std::string& relative_target)
{
    run_cmd(session, fmt::format("sudo /bin/bash -c 'cd \"{}\" && mkdir -p \"{}\"'", root, relative_target));
}

// Set ownership of all directories on a path starting on a given root.
// Assume it is already created.
void set_owner_for(mp::SSHSession& session, const std::string& root, const std::string& relative_target, int vm_user,
                   int vm_group)
{
    run_cmd(session, fmt::format("sudo /bin/bash -c 'cd \"{}\" && chown -R {}:{} \"{}\"'", root, vm_user, vm_group,
                                 relative_target.substr(0, relative_target.find_first_of('/'))));
}

auto make_sftp_server(mp::SSHSession&& session, const std::string& source, const std::string& target,
                      const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map)
{
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(source = {}, target = {}, …): ", __FILE__, __LINE__, __FUNCTION__, source, target));

    auto sshfs_exec_line = get_sshfs_exec_and_options(session);

    // Split the path in existing and missing parts.
    const auto& [leading, missing] = get_path_split(session, target);

    auto output = run_cmd(session, "id -u");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -u` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_uid = std::stoi(output);

    output = run_cmd(session, "id -g");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -g` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_gid = std::stoi(output);

    // We need to create the part of the path which does not still exist,
    // and set then the correct ownership.
    if (missing != ".")
    {
        make_target_dir(session, leading, missing);
        set_owner_for(session, leading, missing, default_uid, default_gid);
    }

    return std::make_unique<mp::SftpServer>(std::move(session), source, leading + missing, gid_map, uid_map,
                                            default_uid, default_gid, sshfs_exec_line);
}

} // namespace

mp::SshfsMount::SshfsMount(SSHSession&& session, const std::string& source, const std::string& target,
                           const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map)
    : sftp_server{make_sftp_server(std::move(session), source, target, gid_map, uid_map)}, sftp_thread{[this] {
          std::cout << "Connected" << std::endl;
          sftp_server->run();
          std::cout << "Stopped" << std::endl;
      }}
{
}

mp::SshfsMount::~SshfsMount()
{
    stop();
}

void mp::SshfsMount::stop()
{
    sftp_server->stop();
    if (sftp_thread.joinable())
        sftp_thread.join();
}
