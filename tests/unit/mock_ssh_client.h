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

#pragma once

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/cli/return_codes.h>
#include <multipass/ssh/client_ssh_factory.h>
#include <multipass/ssh/ssh_client.h>

#include <string>
#include <vector>

namespace multipass::test
{
// TODO@sshclient: document MockSSHClient
class MockSSHClient : public SSHClient
{
public:
    MOCK_METHOD(VMReturnCode,
                exec,
                (const std::vector<std::vector<std::string>>&),
                (override));
    MOCK_METHOD(VMReturnCode, open_shell, (), (override));
};

// TODO@sshclient: document MockClientSSHFactory
class MockClientSSHFactory : public ClientSSHFactory
{
public:
    using ClientSSHFactory::ClientSSHFactory;

    MOCK_METHOD(SSHClientUPtr,
                make_ssh_client,
                (const SSHCoordinates&, Terminal*),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockClientSSHFactory, ClientSSHFactory);
};
} // namespace multipass::test
