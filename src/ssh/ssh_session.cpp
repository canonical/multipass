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

#include <multipass/ssh/ssh_session.h>

#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/ssh/throw_on_error.h>

#include <libssh/callbacks.h>
#include <libssh/socket.h>

#include <multipass/format.h>

#include <QStandardPaths>

#include <sstream>
#include <stdexcept>
#include <string>

namespace mp = multipass;

mp::SSHSession::SSHSession(const std::string& host, int port, const std::string& username,
                           const SSHKeyProvider* key_provider, const std::chrono::milliseconds timeout)
    : session{ssh_new(), ssh_free}
{
    if (session == nullptr)
        throw std::runtime_error("could not allocate ssh session");

    const long timeout_secs = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
    const int nodelay{1};
    auto ssh_dir =
        QString("%1/.ssh").arg(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).toStdString();

    set_option(SSH_OPTIONS_HOST, host.c_str());
    set_option(SSH_OPTIONS_PORT, &port);
    set_option(SSH_OPTIONS_USER, username.c_str());
    set_option(SSH_OPTIONS_TIMEOUT, &timeout_secs);
    set_option(SSH_OPTIONS_NODELAY, &nodelay);
    set_option(SSH_OPTIONS_CIPHERS_C_S, "chacha20-poly1305@openssh.com,aes256-ctr");
    set_option(SSH_OPTIONS_CIPHERS_S_C, "chacha20-poly1305@openssh.com,aes256-ctr");
    set_option(SSH_OPTIONS_SSH_DIR, ssh_dir.c_str());

    SSH::throw_on_error(session, "ssh connection failed", ssh_connect);
    if (key_provider)
    {
        SSH::throw_on_error(session, "ssh failed to authenticate", ssh_userauth_publickey, nullptr,
                            key_provider->private_key());
    }
}

mp::SSHSession::SSHSession(const std::string& host, int port, const std::string& username,
                           const SSHKeyProvider& key_provider, const std::chrono::milliseconds timeout)
    : SSHSession(host, port, username, &key_provider, timeout)
{
}

mp::SSHSession::SSHSession(const std::string& host, int port, const std::chrono::milliseconds timeout)
    : SSHSession(host, port, "ubuntu", nullptr, timeout)
{
}

mp::SSHProcess mp::SSHSession::exec(const std::string& cmd)
{
    return {session.get(), cmd};
}

void mp::SSHSession::force_shutdown()
{
    auto socket = ssh_get_fd(session.get());

    const int shutdown_read_and_writes = 2;
    shutdown(socket, shutdown_read_and_writes);
}

mp::SSHSession::operator ssh_session() const
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

void mp::SSHSession::set_option(ssh_options_e type, const void* data)
{
    const auto ret = ssh_options_set(session.get(), type, data);
    if (ret != SSH_OK)
    {
        throw std::runtime_error(fmt::format("libssh failed to set {} option to '{}': '{}'", name_for(type),
                                             as_string(type, data), ssh_get_error(session.get())));
    }
}
