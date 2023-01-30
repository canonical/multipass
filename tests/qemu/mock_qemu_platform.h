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

#ifndef MULTIPASS_MOCK_QEMU_PLATFORM_H
#define MULTIPASS_MOCK_QEMU_PLATFORM_H

#include "tests/common.h"
#include "tests/mock_singleton_helpers.h"

#include <src/platform/backends/qemu/qemu_platform.h>

namespace multipass
{
namespace test
{
struct MockQemuPlatform : public QemuPlatform
{
    MockQemuPlatform()
    {
        EXPECT_CALL(*this, vmstate_platform_args())
            .Times(testing::AnyNumber())
            .WillRepeatedly(testing::Return(QStringList()));
    }

    MOCK_METHOD1(get_ip_for, std::optional<IPAddress>(const std::string&));
    MOCK_METHOD1(remove_resources_for, void(const std::string&));
    MOCK_METHOD0(platform_health_check, void());
    MOCK_METHOD0(vmstate_platform_args, QStringList());
    MOCK_METHOD1(vm_platform_args, QStringList(const VirtualMachineDescription&));
    MOCK_METHOD0(get_directory_name, QString());
};

struct MockQemuPlatformFactory : public QemuPlatformFactory
{
    using QemuPlatformFactory::QemuPlatformFactory;

    MOCK_CONST_METHOD1(make_qemu_platform, QemuPlatform::UPtr(const Path&));

    MP_MOCK_SINGLETON_BOILERPLATE(MockQemuPlatformFactory, QemuPlatformFactory);
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_QEMU_PLATFORM
