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
#include <src/platform/backends/hyperv/hyperv_virtual_machine_factory.h>

#include "stub_ssh_key_provider.h"
#include "stub_status_monitor.h"
#include "temp_file.h"

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct HyperVBackend : public testing::Test
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::StubSSHKeyProvider key_provider;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used,
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name(),
                                                      &key_provider};
    mp::HyperVVirtualMachineFactory backend;
};
} // namespace

TEST_F(HyperVBackend, creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    ASSERT_THAT(machine.get(), NotNull());
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}
