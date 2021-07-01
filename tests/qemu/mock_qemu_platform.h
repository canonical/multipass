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

#ifndef MULTIPASS_MOCK_QEMU_PLATFORM_H
#define MULTIPASS_MOCK_QEMU_PLATFORM_H

#include <src/platform/backends/qemu/qemu_platform.h>

#include <gmock/gmock.h>

namespace multipass
{
namespace test
{
struct MockQemuPlatform : public QemuPlatform
{
    using QemuPlatform::QemuPlatform; // ctor

    MOCK_METHOD1(get_ip_for, optional<IPAddress>(const std::string&));
    MOCK_METHOD0(platform_health_check, void());
    MOCK_METHOD2(qemu_netdev, QString(const std::string&, const std::string&));
    MOCK_METHOD0(qemu_platform_args, QStringList());
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_QEMU_PLATFORM
