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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/ssh/ssh_session.h>

#include <multipass/ssh/throw_on_error.h>
#include <multipass/ssh/ssh_key_provider.h>

#include <libssh/callbacks.h>
#include <libssh/socket.h>

#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace mp = multipass;

namespace
{
int mutex_init(void** priv)
{
    auto mutex = new std::mutex();
    *priv = mutex;
    return 0;
}

int mutex_destroy(void** lock)
{
    auto mutex = reinterpret_cast<std::mutex*>(*lock);
    delete mutex;
    return 0;
}

int mutex_lock(void** lock)
{
    auto mutex = reinterpret_cast<std::mutex*>(*lock);
    mutex->lock();
    return 0;
}

int mutex_unlock(void** lock)
{
    auto mutex = reinterpret_cast<std::mutex*>(*lock);
    mutex->unlock();
    return 0;
}

unsigned long thread_id()
{
    // libssh + boringSSL do not use the given thread id
    return 0;
}

void init_ssh()
{
    static struct ssh_threads_callbacks_struct mutex_wrappers
    {
        "cpp11_mutex", mutex_init, mutex_destroy, mutex_lock, mutex_unlock, thread_id
    };
    ssh_threads_set_callbacks(&mutex_wrappers);
    ssh_init();
}

auto initialize_session()
{
    static std::once_flag flag;
    std::call_once(flag, init_ssh);

    return ssh_new();
}
}

mp::SSHSession::SSHSession(const std::string& host, int port, const std::string& username,
                           const SSHKeyProvider* key_provider)
    : session{initialize_session(), ssh_free}
{
    if (session == nullptr)
        throw std::runtime_error("Could not allocate ssh session");

    const long timeout{1};
    const int nodelay{1};

    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_HOST, host.c_str());
    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_PORT, &port);
    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_USER, username.c_str());
    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_TIMEOUT, &timeout);
    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_NODELAY, &nodelay);
    SSH::throw_on_error(ssh_connect, session);
    if (key_provider)
        SSH::throw_on_error(ssh_userauth_publickey, session, nullptr, key_provider->private_key());
}

mp::SSHSession::SSHSession(const std::string& host, int port, const std::string& username,
                           const SSHKeyProvider& key_provider)
    : SSHSession(host, port, username, &key_provider)
{
}

mp::SSHSession::SSHSession(const std::string& host, int port) : SSHSession(host, port, "ubuntu", nullptr)
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
