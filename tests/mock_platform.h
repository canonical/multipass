/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_PLATFORM_H
#define MULTIPASS_MOCK_PLATFORM_H

#include "mock_singleton_helpers.h"

#include <multipass/platform.h>

#include <gmock/gmock.h>

namespace multipass::test
{
class MockPlatform : public platform::Platform
{
public:
    using Platform::Platform;
    MOCK_CONST_METHOD0(get_network_interfaces_info, std::map<std::string, NetworkInterfaceInfo>());
    MOCK_METHOD1(is_remote_supported, bool(const std::string&));
    MOCK_METHOD2(is_alias_supported, bool(const std::string&, const std::string&));

    MP_MOCK_SINGLETON_BOILERPLATE(MockPlatform, Platform);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_PLATFORM_H
