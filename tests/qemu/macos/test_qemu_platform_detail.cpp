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

#include "tests/common.h"

#include <src/platform/backends/qemu/macos/qemu_platform_detail.h>

#include <algorithm>
#include <vector>

namespace mp = multipass;

using namespace testing;

namespace
{
struct TestQemuPlatformDetail : public Test
{
    void check_expected_args(const std::vector<QStringList>& expected_args,
                             const QStringList& platform_args)
    {
        if (expected_args.empty())
        {
            EXPECT_TRUE(platform_args.isEmpty());
        }
        else
        {
            for (const auto& args : expected_args)
            {
                auto it = std::search(platform_args.cbegin(),
                                      platform_args.cend(),
                                      args.cbegin(),
                                      args.cend());

                EXPECT_NE(it, platform_args.cend());
            }
        }
    };

    const std::string hw_addr{"52:54:00:6f:29:7e"};
    const QString host_arch{HOST_ARCH};
    mp::QemuPlatformDetail qemu_platform_detail;
};
} // namespace

TEST_F(TestQemuPlatformDetail, vmPlatformArgsReturnsExpectedArguments)
{
    std::vector<QStringList> expected_args{
        {"-accel", "hvf"},
        {"-nic",
         QString("vmnet-shared,model=virtio-net-pci,mac=%1").arg(QString::fromStdString(hw_addr))},
        {"-cpu", "host"}};
    mp::VirtualMachineDescription vm_desc;
    vm_desc.vm_name = "foo";
    vm_desc.default_mac_address = hw_addr;

    if (host_arch == "aarch64")
    {
        expected_args.push_back(QStringList({"-machine", "virt,gic-version=3"}));
    }

    check_expected_args(expected_args, qemu_platform_detail.vm_platform_args(vm_desc));
}

TEST_F(TestQemuPlatformDetail, vmstatePlatformArgsReturnsExpectedArguments)
{
    std::vector<QStringList> expected_args;

    if (host_arch == "aarch64")
    {
        expected_args.push_back(QStringList({"-machine", "virt,gic-version=3"}));
    }

    check_expected_args(expected_args, qemu_platform_detail.vmstate_platform_args());
}

TEST_F(TestQemuPlatformDetail, getDirectoryNameReturnsExpectedString)
{
    EXPECT_EQ(qemu_platform_detail.get_directory_name(), "qemu");
}
