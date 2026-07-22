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
#include <multipass/ssh/ssh_factory.h>

namespace mp = multipass;

void mp::SSHKeyDeleter::operator()(ssh_key key) const
{
    ssh_key_free(key);
}

mp::SSHFactory::SSHFactory(const Singleton<SSHFactory>::PrivatePass& pass) noexcept
    : Singleton<SSHFactory>::Singleton{pass}
{
}

mp::SSHKeyUPtr mp::SSHFactory::make_key(const std::string& private_key_as_base64) const
{
    ssh_key priv_key{nullptr};
    ssh_pki_import_privkey_base64(private_key_as_base64.c_str(),
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  &priv_key);

    return mp::SSHKeyUPtr{priv_key};
}

mp::SSHSessionUPtr mp::SSHFactory::make_session(const mp::SSHCoordinates& ssh_coordinates) const
{
    return std::make_unique<mp::PlainSSHSession>(ssh_coordinates);
}
