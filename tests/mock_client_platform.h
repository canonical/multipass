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
 */

#ifndef MULTIPASS_MOCK_CLIENT_PLATFORM_H
#define MULTIPASS_MOCK_CLIENT_PLATFORM_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/cli/client_platform.h>

namespace multipass
{
class Terminal;

namespace test
{
class MockClientPlatform : public cli::platform::Platform
{
public:
    using Platform::Platform;
    MOCK_METHOD((std::string), get_password, (Terminal*), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockClientPlatform, Platform);
};

} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_CLIENT_PLATFORM_H
