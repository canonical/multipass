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

#include <multipass/ssh/plain_ssh_session.h>

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/socket.h>
#include <multipass/ssh/plain_sftp_server_session.h>
#include <multipass/ssh/plain_ssh_process.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/standard_paths.h>
#include <multipass/top_catch_all.h>

#include <QDir>

#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "ssh session";
}

mp::PlainSSHSession::PlainSSHSession(const std::string& host,
                                     int port,
                                     const std::string& username,
                                     const SSHKeyProvider& key_provider)
    : session{ssh_new(), ssh_free}, mut{}
{
    if (session == nullptr)
        throw mp::SSHException("could not allocate ssh session");

    /**
     * Sometimes ssh_connect blocks while a VM is booting up where
     * it won't return to the caller at all. Since the timeout is
     * set to ~infinity the ssh_connect would block forever.
     *
     * The attempt timeout is set as timeout before the ssh_connect
     * in order to prevent that from happening. The established timeout
     * is used afterward to prevent timing out in connected state.
     */
    constexpr long connect_timeout_secs = 5; // < how long to wait for ssh_connect
    constexpr long established_timeout_secs = std::numeric_limits<long>::max();

    const int nodelay{1};
    auto ssh_dir = QDir(MP_STDPATHS.writableLocation(StandardPaths::AppConfigLocation))
                       .filePath("ssh")
                       .toStdString();

    set_option(SSH_OPTIONS_HOST, host.c_str());
    set_option(SSH_OPTIONS_PORT, &port);
    set_option(SSH_OPTIONS_USER, username.c_str());
    set_option(SSH_OPTIONS_TIMEOUT, &connect_timeout_secs);
    set_option(SSH_OPTIONS_NODELAY, &nodelay);
    set_option(SSH_OPTIONS_CIPHERS_C_S, "chacha20-poly1305@openssh.com,aes256-ctr");
    set_option(SSH_OPTIONS_CIPHERS_S_C, "chacha20-poly1305@openssh.com,aes256-ctr");
    set_option(SSH_OPTIONS_SSH_DIR, ssh_dir.c_str());

    SSH::throw_on_error(session, "ssh connection failed", ssh_connect);
    set_option(SSH_OPTIONS_TIMEOUT, &established_timeout_secs);
    SSH::throw_on_error(session,
                        "ssh failed to authenticate",
                        ssh_userauth_publickey,
                        nullptr,
                        key_provider.private_key());
}

mp::PlainSSHSession::PlainSSHSession(PlainSSHSession&& other)
    : PlainSSHSession(std::move(other), std::unique_lock{other.mut})
{
}

mp::PlainSSHSession::PlainSSHSession(PlainSSHSession&& other, std::unique_lock<std::mutex>)
    : session{std::move(other.session)}, mut{}
{
}

mp::PlainSSHSession& mp::PlainSSHSession::operator=(PlainSSHSession&& other)
{
    if (this != &other)
    {
        std::scoped_lock lock{mut, other.mut};
        session = std::move(other.session);
    }

    return *this;
}

mp::PlainSSHSession::~PlainSSHSession()
{
    top_catch_all(category, [this] {
        std::unique_lock lock{mut};
        if (session)
        {
            mpl::trace(category, "disconnecting SSH session");

            ssh_disconnect(session.get());
            PlainSSHSession::force_shutdown(); // Shutdown I/O on manually open sockets.
                                               // The socket is still closed by libssh in ssh_free.
        }
    });
}

std::unique_ptr<mp::SSHProcess> mp::PlainSSHSession::exec(const std::string& cmd, bool whisper)
{
    std::unique_lock lock{mut};
    assert(!is_moved() && "precondition - cannot call exec on a moved session");

    auto lvl = whisper ? mpl::Level::trace : mpl::Level::debug;
    mpl::log(lvl, category, "Executing '{}'", cmd);

    return std::make_unique<PlainSSHProcess>(*session.get(), cmd, std::move(lock));
}

std::unique_ptr<mp::SftpServerSession> mp::PlainSSHSession::make_sftp_server_session() &&
{
    return std::make_unique<PlainSftpServerSession>(
        std::move(*this),
        std::string{}); // TODO@sftp proper sshfs command
}

void mp::PlainSSHSession::force_shutdown()
{
    // TODO@sftp This is public but doesn't lock (it can't, because it is also called internally
    // with a lock acquired). Make it private instead. Provide public way to close the session
    // (probably just the dtor - let outside callers delete and deal with null session)
    if (!session)
        return;

    if (auto socket = ssh_get_fd(session.get()); socket != -1)
        MP_PLATFORM.shutdown_socket(socket);
}
ssh_session multipass::PlainSSHSession::borrow_session(
    const PrivatePassProvider<PlainSftpServerSession>::PrivatePass&) const
{
    return session.get();
}

mp::PlainSSHSession::operator ssh_session()
{
    return session.get();
}

namespace
{
const char* name_for(ssh_options_e type)
{
    switch (type)
    {
    case SSH_OPTIONS_HOST:
        return "host";
    case SSH_OPTIONS_PORT:
        return "port";
    case SSH_OPTIONS_USER:
        return "username";
    case SSH_OPTIONS_TIMEOUT:
        return "timeout";
    case SSH_OPTIONS_NODELAY:
        return "NODELAY";
    case SSH_OPTIONS_CIPHERS_C_S:
        return "client to server ciphers";
    case SSH_OPTIONS_CIPHERS_S_C:
        return "server to client ciphers";
    case SSH_OPTIONS_SSH_DIR:
        return "ssh config directory";
    default:
        break;
    }
    return "unknown";
}

std::string as_string(ssh_options_e type, const void* value)
{
    switch (type)
    {
    case SSH_OPTIONS_HOST:
    case SSH_OPTIONS_USER:
    case SSH_OPTIONS_CIPHERS_C_S:
    case SSH_OPTIONS_CIPHERS_S_C:
    case SSH_OPTIONS_SSH_DIR:
        return std::string(reinterpret_cast<const char*>(value));
    case SSH_OPTIONS_PORT:
    case SSH_OPTIONS_NODELAY:
        return std::to_string(*reinterpret_cast<const int*>(value));
    case SSH_OPTIONS_TIMEOUT:
        return std::to_string(*reinterpret_cast<const long*>(value));
    default:
        break;
    }
    return fmt::format("{}", value);
}

} // namespace

void mp::PlainSSHSession::set_option(ssh_options_e type, const void* data)
{
    std::unique_lock lock{mut};
    assert(session && "should not set option on null session");

    const auto ret = ssh_options_set(session.get(), type, data);
    if (ret != SSH_OK)
    {
        throw mp::SSHException(fmt::format("libssh failed to set {} option to '{}': '{}'",
                                           name_for(type),
                                           as_string(type, data),
                                           ssh_get_error(session.get())));
    }
}

bool mp::PlainSSHSession::is_connected() const
{
    std::unique_lock lock{mut};
    return session && static_cast<bool>(ssh_is_connected(session.get()));
}

bool mp::PlainSSHSession::is_moved() const
{
    return !session;
}
