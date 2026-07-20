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

#include <multipass/ssh/ssh_factory.h>

namespace multipass::test
{
class MockSSHFactory : public SSHFactory
{
public:
    using SSHFactory::SSHFactory;

    MOCK_METHOD(KeyUPtr, make_key, (const std::string&), (const, override));
    MOCK_METHOD(SSHSessionUPtr, make_session, (const SSHCoordinates&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockSSHFactory, SSHFactory);
};
} // namespace multipass::test
