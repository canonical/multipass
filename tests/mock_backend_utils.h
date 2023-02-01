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

    MOCK_METHOD1(create_bridge_with, std::string(const std::string&));
    MOCK_CONST_METHOD2(get_subnet, std::string(const Path&, const QString&));
    MOCK_METHOD0(check_for_kvm_support, void());
    MOCK_METHOD0(check_if_kvm_is_in_use, void());

    MP_MOCK_SINGLETON_BOILERPLATE(MockBackend, Backend);
};

class MockLinuxSysCalls : public LinuxSysCalls
{
public:
    using LinuxSysCalls::LinuxSysCalls;

    MOCK_CONST_METHOD1(close, int(int));
    MOCK_CONST_METHOD3(ioctl, int(int, unsigned long, unsigned long));
    MOCK_CONST_METHOD2(open, int(const char*, mode_t));

    MP_MOCK_SINGLETON_BOILERPLATE(MockLinuxSysCalls, LinuxSysCalls);
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_BACKEND_UTILS_H
