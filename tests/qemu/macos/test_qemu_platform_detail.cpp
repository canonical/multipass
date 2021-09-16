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

#include "tests/common.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/temp_dir.h"

#include <src/platform/backends/qemu/macos/qemu_platform_detail.h>

#include <algorithm>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestQemuPlatformDetail : public Test
{
    const std::string hw_addr{"52:54:00:6f:29:7e"};
    const QString host_arch{HOST_ARCH};
    mp::QemuPlatformDetail qemu_platform_detail;
};
} // namespace

TEST_F(TestQemuPlatformDetail, get_ip_for_returns_expected_info)
{
    const QString ip_addr{"192.168.64.5"};
    const mp::IPAddress ip_address{ip_addr.toStdString()};
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, read_line(_))
        .WillOnce(Return("{"))
        .WillOnce(Return("    name=foo"))
        .WillOnce(Return(QString("    ip_address=%1").arg(ip_addr)))
        .WillOnce(Return("    hw_address=1,52:54:0:6f:29:7e"));

    auto addr = qemu_platform_detail.get_ip_for(hw_addr);

    EXPECT_EQ(*addr, ip_address);
}

TEST_F(TestQemuPlatformDetail, get_ip_returns_nullopt_not_found)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, read_line(_)).WillOnce(Return(QString()));

    auto addr = qemu_platform_detail.get_ip_for(hw_addr);

    EXPECT_EQ(addr, mp::nullopt);
}

TEST_F(TestQemuPlatformDetail, get_ip_throws_when_no_matching_ip)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, read_line(_))
        .WillOnce(Return("{"))
        .WillOnce(Return("    name=foo"))
        .WillOnce(Return("    hw_address=1,52:54:0:6f:29:7e"))
        .WillOnce(Return("}"));

    MP_EXPECT_THROW_THAT(qemu_platform_detail.get_ip_for(hw_addr), std::runtime_error,
                         mpt::match_what(StrEq("Failed to parse IP address out of the leases file.")));
}

TEST_F(TestQemuPlatformDetail, get_ip_logs_when_unknown_key)
{
    const QString unknown_line{"    power=high"};
    auto logger_scope = mpt::MockLogger::inject();
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, read_line(_))
        .WillOnce(Return("{"))
        .WillOnce(Return(unknown_line))
        .WillOnce(Return("}"))
        .WillOnce(Return(QString()));

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(
        mpl::Level::warning, fmt::format("Got unexpected line when parsing the leases file: {}", unknown_line));

    EXPECT_NO_THROW(qemu_platform_detail.get_ip_for(hw_addr));
}

TEST_F(TestQemuPlatformDetail, vm_platform_args_returns_expected_arguments)
{
    const QStringList accel_args{"-accel", "hvf"},
        nic_args{"-nic",
                 QString("vmnet-macos,mode=shared,model=virtio-net-pci,mac=%1").arg(QString::fromStdString(hw_addr))},
        cpu_args{"-cpu", (host_arch == "aarch64" ? "cortex-a72" : "host")}, common_args{"-machine", "virt,highmem=off"};
    mp::VirtualMachineDescription vm_desc;
    vm_desc.vm_name = "foo";
    vm_desc.default_mac_address = hw_addr;

    auto platform_args = qemu_platform_detail.vm_platform_args(vm_desc);

    auto accel_args_it =
        std::search(platform_args.cbegin(), platform_args.cend(), accel_args.cbegin(), accel_args.cend());

    EXPECT_TRUE(accel_args_it != platform_args.cend());

    auto nic_args_it = std::search(platform_args.cbegin(), platform_args.cend(), nic_args.cbegin(), nic_args.cend());

    EXPECT_TRUE(nic_args_it != platform_args.cend());

    auto cpu_args_it = std::search(platform_args.cbegin(), platform_args.cend(), cpu_args.cbegin(), cpu_args.cend());

    EXPECT_TRUE(cpu_args_it != platform_args.cend());

    auto common_args_it =
        std::search(platform_args.cbegin(), platform_args.cend(), common_args.cbegin(), common_args.cend());

    if (host_arch == "aarch64")
    {
        EXPECT_TRUE(common_args_it != platform_args.cend());
    }
    else
    {
        EXPECT_TRUE(common_args_it == platform_args.cend());
    }
}

TEST_F(TestQemuPlatformDetail, vmstate_platform_args_returns_expected_arguments)
{
    auto vmstate_args = qemu_platform_detail.vmstate_platform_args();

    if (host_arch == "aarch64")
    {
        const QStringList common_args{"-machine", "virt,highmem=off"};

        auto common_args_it =
            std::search(vmstate_args.cbegin(), vmstate_args.cend(), common_args.cbegin(), common_args.cend());

        EXPECT_TRUE(common_args_it != vmstate_args.cend());
    }
    else
    {
        EXPECT_TRUE(vmstate_args.isEmpty());
    }
}

TEST_F(TestQemuPlatformDetail, get_directory_name_returns_expected_string)
{
    EXPECT_EQ(qemu_platform_detail.get_directory_name(), "qemu");
}
