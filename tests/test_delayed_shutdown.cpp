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
#include "mock_ssh_test_fixture.h"
#include "signal.h"
#include "stub_virtual_machine.h"

#include <multipass/delayed_shutdown_timer.h>

#include <QEventLoop>

#include <chrono>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

struct DelayedShutdown : public Test
{
    DelayedShutdown()
    {
        vm = std::make_unique<mpt::StubVirtualMachine>();
        vm->state = mp::VirtualMachine::State::running;
    }

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mp::VirtualMachine::UPtr vm;
    mp::SSHSession session{"a", 42};
    QEventLoop loop;
    ssh_channel_callbacks callbacks{nullptr};
};

TEST_F(DelayedShutdown, emits_finished_after_timer_expires)
{
    mpt::Signal finished;
    mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session), [](const std::string&) {}};

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
    mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session), [](const std::string&) {}};

    QObject::connect(&delayed_shutdown_timer, &mp::DelayedShutdownTimer::finished, [&finished] { finished.signal(); });

    delayed_shutdown_timer.start(std::chrono::milliseconds::zero());
    auto finish_invoked = finished.wait_for(std::chrono::seconds(1));
    EXPECT_TRUE(finish_invoked);
}

TEST_F(DelayedShutdown, vm_state_delayed_shutdown_when_timer_running)
{
    auto add_channel_cbs = [this](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    auto event_dopoll = [this](auto...) {
        if (!callbacks)
            return SSH_ERROR;
        callbacks->channel_exit_status_function(nullptr, nullptr, 0, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);

    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::running);
    mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session), [](const std::string&) {}};
    delayed_shutdown_timer.start(std::chrono::milliseconds(1));

    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::delayed_shutdown);
}

TEST_F(DelayedShutdown, vm_state_running_after_cancel)
{
    auto add_channel_cbs = [this](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    auto event_dopoll = [this](auto...) {
        if (!callbacks)
            return SSH_ERROR;
        callbacks->channel_exit_status_function(nullptr, nullptr, 0, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);

    {
        mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session), [](const std::string&) {}};
        delayed_shutdown_timer.start(std::chrono::milliseconds(1));
        EXPECT_TRUE(vm->state == mp::VirtualMachine::State::delayed_shutdown);
    }

    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::running);
}

TEST_F(DelayedShutdown, ssh_fails_vm_state_unknown)
{

    {
        mp::DelayedShutdownTimer delayed_shutdown_timer{vm.get(), std::move(session), [](const std::string&) {}};
        delayed_shutdown_timer.start(std::chrono::minutes(5));
        EXPECT_TRUE(vm->state == mp::VirtualMachine::State::delayed_shutdown);

        REPLACE(ssh_is_connected, [](auto...) { return false; });
    }

    EXPECT_TRUE(vm->state == mp::VirtualMachine::State::unknown);
}
