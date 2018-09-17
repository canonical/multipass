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

#include <multipass/delayed_shutdown_timer.h>
#include <multipass/logging/log.h>

#include <fmt/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::DelayedShutdownTimer::DelayedShutdownTimer(VirtualMachine* virtual_machine) : virtual_machine{virtual_machine}
{
}

mp::DelayedShutdownTimer::~DelayedShutdownTimer()
{
    if (shutdown_timer.isActive())
    {
        mpl::log(mpl::Level::info, virtual_machine->vm_name, fmt::format("Cancelling delayed shutdown"));
        virtual_machine->state = VirtualMachine::State::running;
    }
}

void mp::DelayedShutdownTimer::start(std::chrono::milliseconds delay)
{
    if (virtual_machine->state == VirtualMachine::State::stopped ||
        virtual_machine->state == VirtualMachine::State::off)
        return;

    if (delay > decltype(delay)(0))
    {
        mpl::log(mpl::Level::info, virtual_machine->vm_name,
                 fmt::format("Shutdown request delayed for {} minute{}",
                             std::chrono::duration_cast<std::chrono::minutes>(delay).count(),
                             delay > std::chrono::minutes(1) ? "s" : ""));
        QObject::connect(&shutdown_timer, &QTimer::timeout, [this]() { shutdown_instance(); });

        virtual_machine->state = VirtualMachine::State::delayed_shutdown;

        shutdown_timer.setSingleShot(true);
        shutdown_timer.start(delay);
    }
    else
    {
        shutdown_instance();
    }
}

void mp::DelayedShutdownTimer::shutdown_instance()
{
    virtual_machine->shutdown();
    emit finished();
}
