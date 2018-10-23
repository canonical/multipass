/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include <src/platform/backends/qemu/qemu_virtual_machine_factory.h>

#include "mock_status_monitor.h"
#include "stub_ssh_key_provider.h"
#include "stub_status_monitor.h"
#include "temp_dir.h"
#include "temp_file.h"
#include "test_with_mocked_bin_path.h"

#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct QemuBackend : public mpt::TestWithMockedBinPath
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::StubSSHKeyProvider key_provider;
    mp::VirtualMachineDescription default_description{2,
                                                      "3M",
                                                      "",
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name(),
                                                      key_provider};
    mpt::TempDir data_dir;
};

TEST_F(QemuBackend, creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machine_persists_sends_monitor_event_and_sets_state_on_start)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_));
    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    EXPECT_CALL(mock_monitor, persist_state_for(_)).Times(AtLeast(1));
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();
}

TEST_F(QemuBackend, throws_when_starting_while_suspending)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    machine->state = mp::VirtualMachine::State::suspending;

    EXPECT_THROW(machine->start(), std::runtime_error);
}
