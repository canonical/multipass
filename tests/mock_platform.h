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

#include <multipass/platform.h>

#include <scope_guard.hpp>

#include <gmock/gmock.h>

#include <utility>

#define MP_SINGLETON_MOCK_INSTANCE(mock_class)                                                                         \
    static mock_class& mock_instance()                                                                                 \
    {                                                                                                                  \
        return dynamic_cast<mock_class&>(instance());                                                                  \
    }

#define MP_SINGLETON_MOCK_INJECT(mock_class, parent_class)                                                             \
    [[nodiscard]] static auto inject()                                                                                 \
    {                                                                                                                  \
        parent_class::reset();                                                                                         \
        parent_class::mock<mock_class>();                                                                              \
        return std::make_pair(&mock_instance(), sg::make_scope_guard([]() { parent_class::reset(); }));                \
    } // one at a time, please!

#define MP_SINGLETON_MOCK_BOILERPLATE(mock_class, parent_class)                                                        \
    MP_SINGLETON_MOCK_INSTANCE(mock_class)                                                                             \
    MP_SINGLETON_MOCK_INJECT(mock_class, parent_class)

namespace multipass::test
{
class MockPlatform : public platform::Platform
{
public:
    using Platform::Platform;
    MOCK_CONST_METHOD0(get_network_interfaces_info, std::map<std::string, NetworkInterfaceInfo>());

    MP_SINGLETON_MOCK_BOILERPLATE(MockPlatform, Platform);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_PLATFORM_H
