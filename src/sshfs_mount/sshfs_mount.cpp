/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <fmt/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "sshfs mount";
template <typename Callable>
auto run_cmd(mp::SSHSession& session, std::string&& cmd, Callable&& error_handler)
{
    auto ssh_process = session.exec(cmd);
    if (ssh_process.exit_code() != 0)
        error_handler(ssh_process);
    return ssh_process.read_std_output();
}

auto run_cmd(mp::SSHSession& session, std::string&& cmd)
{
    auto error_handler = [](mp::SSHProcess& proc) { throw std::runtime_error(proc.read_std_error()); };
    return run_cmd(session, std::forward<std::string>(cmd), error_handler);
}

void check_sshfs_is_running(mp::SSHSession& session, mp::SSHProcess& sshfs_process, const std::string& source,
                            const std::string& target)
{
    using namespace std::literals::chrono_literals;

    // Make sure sshfs actually runs
    std::this_thread::sleep_for(250ms);
    auto error_handler = [&sshfs_process](mp::SSHProcess&) {
        throw std::runtime_error(sshfs_process.read_std_error());
    };
    run_cmd(session, fmt::format("pgrep -fx \"sshfs.*{}.*{}\"", source, target), error_handler);
}

void check_sshfs_exists(mp::SSHSession& session)
{
    auto error_handler = [](mp::SSHProcess&) { throw mp::SSHFSMissingError(); };
    run_cmd(session, "which sshfs", error_handler);
}

void make_target_dir(mp::SSHSession& session, const std::string& target)
{
    run_cmd(session, fmt::format("sudo mkdir -p \"{}\"", target));
}

void set_owner_for(mp::SSHSession& session, const std::string& target)
{
    auto vm_user = run_cmd(session, "id -nu");
    auto vm_group = run_cmd(session, "id -ng");
    mp::utils::trim_end(vm_user);
    mp::utils::trim_end(vm_group);

    run_cmd(session, fmt::format("sudo chown {}:{} \"{}\"", vm_user, vm_group, target));
}

auto create_sshfs_process(mp::SSHSession& session, const std::string& source, const std::string& target)
{
    check_sshfs_exists(session);
    make_target_dir(session, target);
    set_owner_for(session, target);

    auto sshfs_process = session.exec(fmt::format(
        "sudo sshfs -o slave -o nonempty -o transform_symlinks -o allow_other :\"{}\" \"{}\"", source, target));

    check_sshfs_is_running(session, sshfs_process, source, target);

    return sshfs_process;
}

auto make_sftp_server(mp::SSHSession&& session, const std::string& source, const std::string& target,
                      const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map)
{
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(source = {}, target = {}, …): ", __FILE__, __LINE__, __FUNCTION__, source, target));

    auto sshfs_proc =
        create_sshfs_process(session, mp::utils::escape_char(source, '"'), mp::utils::escape_char(target, '"'));

    auto output = run_cmd(session, "id -u");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -u` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_uid = std::stoi(output);
    output = run_cmd(session, "id -g");
    mpl::log(mpl::Level::debug, category,
             fmt::format("{}:{} {}(): `id -g` = {}", __FILE__, __LINE__, __FUNCTION__, output));
    auto default_gid = std::stoi(output);

    return std::make_unique<mp::SftpServer>(std::move(session), std::move(sshfs_proc), source, gid_map, uid_map,
                                            default_uid, default_gid);
}

} // namespace

mp::SshfsMount::SshfsMount(SSHSession&& session, const std::string& source, const std::string& target,
                           const std::unordered_map<int, int>& gid_map, const std::unordered_map<int, int>& uid_map)
    : sftp_server{make_sftp_server(std::move(session), source, target, gid_map, uid_map)}, sftp_thread{[this] {
          sftp_server->run();
          emit finished();
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
