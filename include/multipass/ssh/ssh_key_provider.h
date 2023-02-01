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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_SSH_KEY_PROVIDER_H
#define MULTIPASS_SSH_KEY_PROVIDER_H

#include <multipass/disabled_copy_move.h>

#include <libssh/libssh.h>

#include <string>

namespace multipass
{
class SSHKeyProvider : private DisabledCopyMove
{
public:
    virtual ~SSHKeyProvider() = default;
    virtual std::string private_key_as_base64() const = 0;
    virtual std::string public_key_as_base64() const = 0;
    virtual ssh_key private_key() const = 0;

protected:
    SSHKeyProvider() = default;
};
}
#endif // MULTIPASS_SSH_KEY_PROVIDER_H
