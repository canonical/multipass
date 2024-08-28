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
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/temp_dir.h"

#include <src/platform/backends/qemu/macos/qemu_platform_detail.h>

#include <algorithm>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestQemuPlatformDetail : public Test
{
    void check_expected_args(const std::vector<QStringList>& expected_args, const QStringList& platform_args)
    {
        if (expected_args.empty())
        {
            EXPECT_TRUE(platform_args.isEmpty());
        }
        else
        {
            for (const auto& args : expected_args)
            {
                auto it = std::search(platform_args.cbegin(), platform_args.cend(), args.cbegin(), args.cend());

                EXPECT_NE(it, platform_args.cend());
            }
        }
    };

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

    EXPECT_EQ(addr, std::nullopt);
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
    std::vector<QStringList> expected_args{
        {"-accel", "hvf"},
        {"-nic", QString("vmnet-shared,model=virtio-net-pci,mac=%1").arg(QString::fromStdString(hw_addr))},
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

TEST_F(TestQemuPlatformDetail, vmstate_platform_args_returns_expected_arguments)
{
    std::vector<QStringList> expected_args;

    if (host_arch == "aarch64")
    {
        expected_args.push_back(QStringList({"-machine", "virt,gic-version=3"}));
    }

    check_expected_args(expected_args, qemu_platform_detail.vmstate_platform_args());
}

TEST_F(TestQemuPlatformDetail, get_directory_name_returns_expected_string)
{
    EXPECT_EQ(qemu_platform_detail.get_directory_name(), "qemu");
}
