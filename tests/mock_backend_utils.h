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

#ifndef MULTIPASS_MOCK_BACKEND_UTILS_H
#define MULTIPASS_MOCK_BACKEND_UTILS_H

#include "common.h"
#include "mock_singleton_helpers.h"

#include <src/platform/backends/shared/linux/backend_utils.h>

namespace multipass::test
{
class MockBackend : public Backend
{
public:
    using Backend::Backend;

    MOCK_METHOD(std::string, create_bridge_with, (const std::string&), (override));
    MOCK_METHOD(std::string, get_subnet, (const Path&, const QString&), (const, override));
    MOCK_METHOD(void, check_for_kvm_support, (), (override));
    MOCK_METHOD(void, check_if_kvm_is_in_use, (), (override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockBackend, Backend);
};

class MockLinuxSysCalls : public LinuxSysCalls
{
public:
    using LinuxSysCalls::LinuxSysCalls;

    MOCK_METHOD(int, close, (int), (const, override));
    MOCK_METHOD(int, ioctl, (int, unsigned long, unsigned long), (const, override));
    MOCK_METHOD(int, open, (const char*, mode_t), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockLinuxSysCalls, LinuxSysCalls);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_BACKEND_UTILS_H
