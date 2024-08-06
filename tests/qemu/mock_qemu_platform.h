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

    MOCK_METHOD(std::optional<IPAddress>, get_ip_for, (const std::string&), (override));
    MOCK_METHOD(void, remove_resources_for, (const std::string&), (override));
    MOCK_METHOD(void, platform_health_check, (), (override));
    MOCK_METHOD(QStringList, vmstate_platform_args, (), (override));
    MOCK_METHOD(QStringList, vm_platform_args, (const VirtualMachineDescription&), (override));
    MOCK_METHOD(QString, get_directory_name, (), (const, override));
    MOCK_METHOD(bool, is_network_supported, (const std::string&), (const, override));
    MOCK_METHOD(bool, needs_network_prep, (), (const override));
    MOCK_METHOD(std::string, create_bridge_with, (const NetworkInterfaceInfo&), (const, override));
    MOCK_METHOD(void, set_authorization, (std::vector<NetworkInterfaceInfo>&), (override));
};

struct MockQemuPlatformFactory : public QemuPlatformFactory
{
    using QemuPlatformFactory::QemuPlatformFactory;

    MOCK_METHOD(QemuPlatform::UPtr, make_qemu_platform, (const Path&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockQemuPlatformFactory, QemuPlatformFactory);
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_QEMU_PLATFORM
