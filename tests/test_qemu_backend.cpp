/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <src/platform/backends/qemu/qemu_virtual_machine_factory.h>

#include "mock_status_monitor.h"
#include "path.h"
#include "stub_ssh_key_provider.h"
#include "stub_status_monitor.h"
#include "temp_file.h"

#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <QTemporaryDir>
#include <QTemporaryFile>

#include <gmock/gmock.h>

#include <cstdlib>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
void set_path(const std::string& value)
{
    auto ret = setenv("PATH", value.c_str(), 1);
    if (ret != 0)
    {
        std::string message{"unable to modify PATH env variable err:"};
        message.append(std::to_string(ret));
        throw std::runtime_error(message);
    }
}
}
struct QemuBackend : public testing::Test
{
    void SetUp()
    {
        old_path = getenv("PATH");
        std::string new_path{mpt::mock_bin_path()};
        new_path.append(":");
        new_path.append(old_path);
        set_path(new_path);
    }

    void TearDown()
    {
        set_path(old_path);
    }

    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::StubSSHKeyProvider key_provider;
    mp::VirtualMachineDescription default_description{2,
                                                      "3M",
                                                      "",
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name(),
                                                      key_provider};
    QTemporaryDir data_dir;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};
    std::string old_path;
};

TEST_F(QemuBackend, creates_in_off_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(QemuBackend, machine_sends_monitoring_events)
{
    QTemporaryDir data_dir;
    mp::QemuVirtualMachineFactory backend{data_dir.path()};
    mpt::MockVMStatusMonitor mock_monitor;

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();
}
