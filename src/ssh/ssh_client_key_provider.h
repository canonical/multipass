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

#ifndef MULTIPASS_SSH_CLIENT_KEY_PROVIDER_H
#define MULTIPASS_SSH_CLIENT_KEY_PROVIDER_H

#include <multipass/ssh/ssh_key_provider.h>

#include <memory>

namespace multipass
{
class SSHClientKeyProvider : public SSHKeyProvider
{
public:
    struct KeyDeleter
    {
        void operator()(ssh_key key);
    };
    using KeyUPtr = std::unique_ptr<ssh_key_struct, KeyDeleter>;

    explicit SSHClientKeyProvider(const std::string& priv_key_blob);

    std::string private_key_as_base64() const override;
    std::string public_key_as_base64() const override;
    ssh_key private_key() const override;

private:
    KeyUPtr priv_key;
};
}
#endif // MULTIPASS_SSH_CLIENT_KEY_PROVIDER_H
