/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>
#include <src/platform/backends/hyperkit/hyperkit_virtual_machine.cpp>
#include <src/platform/backends/hyperkit/hyperkit_virtual_machine_factory.h>

#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_file.h"

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct HyperkitBackend : public testing::Test
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mp::HyperkitVirtualMachineFactory backend;
};

TEST_F(HyperkitBackend, creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    ASSERT_THAT(machine.get(), NotNull());
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

typedef std::tuple<std::string /* hw_addr */, std::string /* input */, std::string /* ip_addr */,
                   std::string /* test_name */>
    GetIPParamType;

class GetIPSuite : public testing::TestWithParam<GetIPParamType>
{
};

auto print_param_name(const testing::TestParamInfo<GetIPSuite::ParamType>& info)
{
    return std::get<3>(info.param);
}

const std::vector<GetIPParamType> empty_hw_addr_inputs{
    {"test-hostname", "", "", "empty"},
    {"test-hostname",
     "{\n"
     "        name=other-test-hostname\n"
     "        ip_address=192.168.64.2\n"
     "        hw_address=1,11:22:33:44:55:66\n"
     "        identifier=1,11:22:33:44:55:66\n"
     "        lease=0x0\n"
     "}",
     "", "missing"},
    {"test-hostname",
     "{\n"
     "        name=test-hostname\n"
     "        ip_address=192.168.64.2\n"
     "        hw_address=1,11:22:33:44:55:66\n"
     "        identifier=1,11:22:33:44:55:66\n"
     "        lease=0x0\n"
     "}",
     "192.168.64.2", "matched"},
    {"test-hostname",
     "{\n"
     "        name=other-test-hostname\n"
     "        ip_address=192.168.64.3\n"
     "        hw_address=1,11:22:33:44:55:66\n"
     "        identifier=1,11:22:33:44:55:66\n"
     "        lease=0x0\n"
     "}\n"
     "{\n"
     "        name=test-hostname\n"
     "        ip_address=192.168.64.2\n"
     "        hw_address=1,11:22:33:44:55:66\n"
     "        identifier=1,11:22:33:44:55:66\n"
     "        lease=0x0\n"
     "}",
     "192.168.64.2", "matched_second"},
    {"test-hostname",
     "bad input\n"
     "{\n"
     "        name=test-hostname\n"
     "        ip_address=192.168.64.2\n"
     "        hw_address=1,11:22:33:44:55:66\n"
     "        identifier=1,11:22:33:44:55:66\n"
     "        lease=0x0\n"
     "}",
     "192.168.64.2", "matched_misformatted"},
};

TEST_P(GetIPSuite, returns_expected)
{
    const auto& [lookup, input, ip_addr, test_name] = GetParam();
    Q_UNUSED(test_name); // gcc 7.4 can't do [[maybe_unused]] for structured bindings

    std::istringstream data{input};

    auto ip = get_ip_for(lookup, data);

    if (ip_addr.empty())
        EXPECT_FALSE(ip);
    else
    {
        ASSERT_TRUE(ip);
        EXPECT_EQ(*ip, ip_addr);
    }
}

INSTANTIATE_TEST_SUITE_P(Hyperkit, GetIPSuite, ValuesIn(empty_hw_addr_inputs), print_param_name);

class GetIPThrowingSuite : public testing::TestWithParam<GetIPParamType>
{
};

const std::vector<GetIPParamType> throwing_hw_addr_inputs{
    {"test-hostname",
     "{\n"
     "        name=test-hostname\n"
     "}",
     "", "matched_missing_ip"},
};

TEST_P(GetIPThrowingSuite, throws_on_bad_format)
{
    const auto& [lookup, input, ip_addr, test_name] = GetParam();
    Q_UNUSED(ip_addr);
    Q_UNUSED(test_name); // gcc 7.4 can't do [[maybe_unused]] for structured bindings

    std::istringstream data{input};

    EXPECT_THROW(get_ip_for(lookup, data), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(Hyperkit, GetIPThrowingSuite, ValuesIn(throwing_hw_addr_inputs), print_param_name);
