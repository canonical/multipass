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

#include "ssh_client_key_provider.h"

#include <stdexcept>

namespace mp = multipass;

namespace
{
mp::SSHClientKeyProvider::KeyUPtr import_priv_key(const std::string& priv_key_blob)
{
    ssh_key priv_key;
    ssh_pki_import_privkey_base64(priv_key_blob.c_str(), nullptr, nullptr, nullptr, &priv_key);

    return mp::SSHClientKeyProvider::KeyUPtr{priv_key};
}
}

void mp::SSHClientKeyProvider::KeyDeleter::operator()(ssh_key key)
{
    ssh_key_free(key);
}

mp::SSHClientKeyProvider::SSHClientKeyProvider(const std::string& priv_key_blob)
    : priv_key{import_priv_key(priv_key_blob)}
{
}

std::string mp::SSHClientKeyProvider::private_key_as_base64() const
{
    throw std::runtime_error("Unimplemented");
}

std::string mp::SSHClientKeyProvider::public_key_as_base64() const
{
    throw std::runtime_error("Unimplemented");
}

ssh_key mp::SSHClientKeyProvider::private_key() const
{
    return priv_key.get();
}
