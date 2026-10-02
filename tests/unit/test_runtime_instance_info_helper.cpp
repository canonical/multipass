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

#include "common.h"
#include "mock_virtual_machine.h"

#include <src/daemon/runtime_instance_info_helper.h>

#include <multipass/rpc/multipass.grpc.pb.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
constexpr auto runtime_info_output = "loadavg: 0.01 0.05 0.10\n"
                                     "mem_usage: 60817408\n"
                                     "mem_total: 1024000000\n"
                                     "disk_usage: 1288490188\n"
                                     "disk_total: 5153960755\n"
                                     "cpus: 2\n"
                                     "cpu_times: cpu 1 2 3 4 5 6 7 8 9 10\n"
                                     "uptime: 2 hours, 5 minutes\n"
                                     "current_release: Ubuntu 26.04.1 LTS\n";

struct TestRuntimeInstanceInfoHelper : public TestWithParam<bool>
{
    NiceMock<mpt::MockVirtualMachine> vm{mp::VirtualMachine::State::running};
    mp::DetailedInfoItem info;
    mp::InstanceDetails instance_info;
};

TEST_P(TestRuntimeInstanceInfoHelper, usesCLocaleForAllCommands)
{
    // Commands like `free` translate their output in guests with a non-English locale, which
    // breaks parsing it (e.g. `free` prints "内存：" instead of "Mem:" in zh_CN), so the C locale
    // must be set before running any of them.
    EXPECT_CALL(vm, ssh_exec(StartsWith("export LC_ALL=C; "), true))
        .WillOnce(Return(runtime_info_output));

    mp::RuntimeInstanceInfoHelper::populate_runtime_info(vm,
                                                         &info,
                                                         &instance_info,
                                                         "Ubuntu 26.04 LTS",
                                                         GetParam());
}

TEST_P(TestRuntimeInstanceInfoHelper, populatesRuntimeInfo)
{
    EXPECT_CALL(vm, ssh_exec(_, true)).WillOnce(Return(runtime_info_output));

    mp::RuntimeInstanceInfoHelper::populate_runtime_info(vm,
                                                         &info,
                                                         &instance_info,
                                                         "Ubuntu 26.04 LTS",
                                                         GetParam());

    EXPECT_EQ(instance_info.load(), "0.01 0.05 0.10");
    EXPECT_EQ(instance_info.memory_usage(), "60817408");
    EXPECT_EQ(info.memory_total(), "1024000000");
    EXPECT_EQ(instance_info.disk_usage(), "1288490188");
    EXPECT_EQ(info.disk_total(), "5153960755");
    EXPECT_EQ(info.cpu_count(), "2");
    EXPECT_EQ(instance_info.cpu_times(), "cpu 1 2 3 4 5 6 7 8 9 10");
    EXPECT_EQ(instance_info.uptime(), "2 hours, 5 minutes");
    EXPECT_EQ(instance_info.current_release(), "Ubuntu 26.04.1 LTS");
}

INSTANTIATE_TEST_SUITE_P(TestRuntimeInstanceInfoHelper,
                         TestRuntimeInstanceInfoHelper,
                         Values(false, true));
} // namespace
