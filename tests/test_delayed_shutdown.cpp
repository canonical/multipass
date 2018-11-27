/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "mock_ssh.h"
#include "signal.h"
#include "stub_virtual_machine.h"

#include <multipass/delayed_shutdown_timer.h>

#include <QEventLoop>

#include <chrono>
#include <memory>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

struct DelayedShutdown : public Test
{
    DelayedShutdown()
    {
        connect.returnValue(SSH_OK);
        is_connected.returnValue(true);
        open_session.returnValue(SSH_OK);

        vm = std::make_unique<mpt::StubVirtualMachine>();
        vm->state = mp::VirtualMachine::State::running;
    }

    decltype(MOCK(ssh_connect)) connect{MOCK(ssh_connect)};
    decltype(MOCK(ssh_is_connected)) is_connected{MOCK(ssh_is_connected)};
    decltype(MOCK(ssh_channel_open_session)) open_session{MOCK(ssh_channel_open_session)};

    mp::VirtualMachine::UPtr vm;
    mp::SSHSession session{"a", 42};
    QEventLoop loop;
};

TEST_F(DelayedShutdown, emits_finished_after_timer_expires)
{
    mpt::Signal finished;
    mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session)};

    QObject::connect(&delayed_shutdown_timer, &mp::DelayedShutdownTimer::finished, [this, &finished] {
        loop.quit();
        finished.signal();
    });

    delayed_shutdown_timer.start(std::chrono::milliseconds(1));
    loop.exec();

    auto finish_invoked = finished.wait_for(std::chrono::seconds(1));
    EXPECT_TRUE(finish_invoked);
}

TEST_F(DelayedShutdown, emits_finished_with_no_timer)
{
    mpt::Signal finished;
    mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session)};

    QObject::connect(&delayed_shutdown_timer, &mp::DelayedShutdownTimer::finished, [&finished] { finished.signal(); });

    delayed_shutdown_timer.start(std::chrono::milliseconds::zero());
    auto finish_invoked = finished.wait_for(std::chrono::seconds(1));
    EXPECT_TRUE(finish_invoked);
}

TEST_F(DelayedShutdown, vm_state_delayed_shutdown_when_timer_running)
{
    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::running);
    mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session)};
    delayed_shutdown_timer.start(std::chrono::milliseconds(1));

    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::delayed_shutdown);
}

TEST_F(DelayedShutdown, vm_state_running_after_cancel)
{
    {
        mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session)};
        delayed_shutdown_timer.start(std::chrono::milliseconds(1));
        EXPECT_TRUE(vm->state == mp::VirtualMachine::State::delayed_shutdown);
    }

    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::running);
}
