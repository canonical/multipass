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

#include <multipass/sshfs_mount/sshfs_mount.h>

#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/sshfs_mount/sftp_server.h>
#include <multipass/utils.h>

#include <multipass/format.h>

#include <QDir>
#include <iostream>
#include <sstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs mount";
const std::string fuse_version_string{"FUSE library version "};
const std::string ld_library_path_key{"LD_LIBRARY_PATH="};
const std::string snap_path_key{"SNAP="};

auto run_cmd(mp::SSHSession& session, std::string&& cmd)
{
    auto ssh_process = session.exec(cmd);
    if (ssh_process.exit_code() != 0)
        throw std::runtime_error(ssh_process.read_std_error());

    return ssh_process.read_std_output();
}

auto match_line(std::istringstream& output, const std::string& matcher)
{
    output.seekg(0, output.beg);

    std::string line;
    while (std::getline(output, line, '\n'))
    {
        if (line.find(matcher) != std::string::npos)
        {
            return line;
        }
    }

    return std::string{};
}

auto get_sshfs_exec_and_options(mp::SSHSession& session)
{
    std::string sshfs_exec;

    try
    {
        // Prefer to use Snap package version first
        std::istringstream sshfs_env{run_cmd(session, "sudo multipass-sshfs.env")};

        auto ld_library_path = match_line(sshfs_env, ld_library_path_key);
        auto snap_path = match_line(sshfs_env, snap_path_key);
        snap_path = snap_path.substr(snap_path_key.length());

        sshfs_exec = fmt::format("{} {}/bin/sshfs", ld_library_path, snap_path);
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::debug, category, fmt::format("'sshfs' snap package is not installed: {}", e.what()));

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

    std::string fuse_version;
    std::istringstream version_info{run_cmd(session, fmt::format("sudo {} -V", sshfs_exec))};

    auto fuse_version_line = match_line(version_info, fuse_version_string);
    if (!fuse_version_line.empty())
    {
        fuse_version = fuse_version_line.substr(fuse_version_string.length());
    }

    sshfs_exec += " -o slave -o transform_symlinks -o allow_other";

    // The option was removed in libfuse 3.0
    if (fuse_version < "3")
        sshfs_exec += " -o nonempty";

    return sshfs_exec;
}

// Split a path into existing and to-be-created parts.
std::pair<std::string, std::string> get_path_split(mp::SSHSession& session, const std::string& target)
{
    QDir complete_path(QString::fromStdString(target));
    QString absolute;

    if (complete_path.isRelative())
    {
        QString home = QString::fromStdString(run_cmd(session, "pwd")).trimmed();
        absolute = home + '/' + complete_path.path();
    }
    else
    {
        absolute = complete_path.path();
    }

    QString existing =
        QString::fromStdString(
            run_cmd(session,
                    fmt::format("sudo /bin/bash -c 'P=\"{}\"; while [ ! -d \"$P/\" ]; do P=${{P%/*}}; done; echo $P/'",
                                absolute)))
            .trimmed();

    return {existing.toStdString(), QDir(existing).relativeFilePath(absolute).toStdString()};
}

// Create a directory on a given root folder.
void make_target_dir(mp::SSHSession& session, const std::string& root, const std::string& relative_target)
{
    if (!relative_target.empty())
        run_cmd(session, fmt::format("sudo /bin/bash -c 'cd \"{}\" && mkdir -p \"{}\"'", root, relative_target));
}

// Set ownership of all directories on a path starting on a given root.
// Assume it is already created.
void set_owner_for(mp::SSHSession& session, const std::string& root, const std::string& relative_target)
{
    auto vm_user = run_cmd(session, "id -u");
    auto vm_group = run_cmd(session, "id -g");
    mp::utils::trim_end(vm_user);
    mp::utils::trim_end(vm_group);

    run_cmd(session, fmt::format("sudo /bin/bash -c 'cd \"{}\" && chown -R {}:{} {}'", root, vm_user, vm_group,
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

    // We need to create the part of the path which does not still exist,
    // and set then the correct ownership.
    make_target_dir(session, leading, missing);
    set_owner_for(session, leading, missing);

    auto output = run_cmd(session, "id -u");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -u` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_uid = std::stoi(output);
    output = run_cmd(session, "id -g");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -g` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_gid = std::stoi(output);

    return std::make_unique<mp::SftpServer>(std::move(session), source, target, gid_map, uid_map, default_uid,
                                            default_gid, sshfs_exec_line);
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
