/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
#include <src/platform/backends/hyperv/hyperv_virtual_machine_factory.h>

#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_file.h"

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
constexpr auto psexe = "powershell.exe";

struct HyperVBackend : public testing::Test
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used,
                                                      "pied-piper-valley",
                                                      {"default", "", true},
                                                      {},
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mp::HyperVVirtualMachineFactory backend;
};
} // namespace

TEST_F(HyperVBackend, DISABLED_creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    ASSERT_THAT(machine.get(), NotNull());
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(HyperVBackend, list_networks_requests_switches)
{
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    std::unique_ptr<mpt::MockProcessFactory::Scope> factory_scope = mpt::MockProcessFactory::Inject();

    bool forked = false;
    factory_scope->register_callback([&forked](auto* process) {
        ASSERT_EQ(process->program(), psexe);
        EXPECT_THAT(process->arguments(), Contains("Get-VMSwitch"));
        EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));

        forked = true;
    });

    backend.list_networks();
    ASSERT_TRUE(forked);
}
