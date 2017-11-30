/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include <array>
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

mp::SSHSession::SSHSession(const std::string& host, int port, const SSHKeyProvider* key_provider)
    : session{initialize_session(), ssh_free}
{
    if (session == nullptr)
        throw std::runtime_error("Could not allocate ssh session");

    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_HOST, host.c_str());
    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_PORT, &port);
    SSH::throw_on_error(ssh_options_set, session, SSH_OPTIONS_USER, "ubuntu");
    SSH::throw_on_error(ssh_connect, session);
    if (key_provider)
        SSH::throw_on_error(ssh_userauth_publickey, session, nullptr, key_provider->private_key());
}

mp::SSHSession::SSHSession(const std::string& host, int port, const SSHKeyProvider& key_provider)
    : SSHSession(host, port, &key_provider)
{
}

mp::SSHSession::SSHSession(const std::string& host, int port) : SSHSession(host, port, nullptr)
{
}

mp::SSHProcess mp::SSHSession::exec(const std::vector<std::string>& args, const mp::utils::QuoteType quote_type)
{
    if (!ssh_is_connected(session.get()))
        throw std::runtime_error("SSH session is not connected");

    SSHProcess ssh_process{session.get()};
    ssh_process.exec(to_cmd(args, quote_type));
    return ssh_process;
}

void mp::SSHSession::wait_until_ssh_up(const std::string& host, int port, std::chrono::milliseconds timeout,
                                       std::function<void()> precondition_check)
{
    using namespace std::literals::chrono_literals;

    auto deadline = std::chrono::steady_clock::now() + timeout;
    bool ssh_up{false};
    while (std::chrono::steady_clock::now() < deadline)
    {
        precondition_check();

        try
        {
            mp::SSHSession session{host, port};
            ssh_up = true;
            break;
        }
        catch (const std::exception&)
        {
            std::this_thread::sleep_for(1s);
        }
    }

    if (!ssh_up)
        throw std::runtime_error("timed out waiting for ssh service to start");
}

ssh_session mp::SSHSession::get_ssh_session_ptr()
{
    return session.get();
}
