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

#ifndef MULTIPASS_DUMMY_SSH_KEY_PROVIDER_H
#define MULTIPASS_DUMMY_SSH_KEY_PROVIDER_H

#include "stub_ssh_key_provider.h"

namespace multipass
{
namespace test
{
class DummyKeyProvider : public StubSSHKeyProvider
{
public:
    explicit DummyKeyProvider(std::string key) : key{std::move(key)}
    {
    }
    std::string public_key_as_base64() const override
    {
        return key;
    };

private:
    std::string key;
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_DUMMY_SSH_KEY_PROVIDER_H
