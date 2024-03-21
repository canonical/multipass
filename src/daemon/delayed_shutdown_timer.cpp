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

#include <multipass/delayed_shutdown_timer.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/top_catch_all.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
template <typename T>
bool num_plural(const T& num)
{
    return num != T{1}; // use plural for 0 and 2+
}

void attempt_ssh_exec(mp::VirtualMachine& vm, const std::string& cmd)
{
    try
    {
        vm.ssh_exec(cmd);
    }
    catch (const mp::SSHException& e)
    {
        mpl::log(mpl::Level::info, vm.vm_name, fmt::format("Could not broadcast shutdown message in VM: {}", e.what()));
    }
}

void write_shutdown_message(mp::VirtualMachine& vm, const std::chrono::milliseconds& time_left)
{
    if (time_left > std::chrono::milliseconds::zero())
    {
        auto minutes_left = std::chrono::duration_cast<std::chrono::minutes>(time_left).count();

        attempt_ssh_exec(vm,
                         fmt::format("wall \"The system is going down for poweroff in {} minute{}, use 'multipass stop "
                                     "--cancel {}' to cancel the shutdown.\"",
                                     minutes_left,
                                     num_plural(minutes_left) ? "s" : "",
                                     vm.vm_name));
    }
    else
    {
        attempt_ssh_exec(vm, "wall The system is going down for poweroff now");
    }
}
} // namespace

mp::DelayedShutdownTimer::DelayedShutdownTimer(VirtualMachine* virtual_machine, const StopMounts& stop_mounts)
    : virtual_machine{virtual_machine}, stop_mounts{stop_mounts}, time_remaining{0}
{
}

mp::DelayedShutdownTimer::~DelayedShutdownTimer()
{
    mp::top_catch_all(virtual_machine->vm_name, [this] {
        if (shutdown_timer.isActive())
        {
            shutdown_timer.stop();
            mpl::log(mpl::Level::info, virtual_machine->vm_name, "Cancelling delayed shutdown");
            virtual_machine->state = VirtualMachine::State::running;
            attempt_ssh_exec(*virtual_machine, "wall The system shutdown has been cancelled");
        }
    });
}

void mp::DelayedShutdownTimer::start(const std::chrono::milliseconds delay)
{
    if (virtual_machine->state == VirtualMachine::State::stopped ||
        virtual_machine->state == VirtualMachine::State::off)
        return;

    if (delay > decltype(delay)(0))
    {
        auto minutes_left = std::chrono::duration_cast<std::chrono::minutes>(delay).count();
        mpl::log(mpl::Level::info,
                 virtual_machine->vm_name,
                 fmt::format("Shutdown request delayed for {} minute{}", // TODO say "under a minute" if < 1 minute
                             minutes_left,
                             num_plural(minutes_left) ? "s" : ""));
        write_shutdown_message(*virtual_machine, delay);

        time_remaining = delay;
        std::chrono::minutes time_elapsed{1};
        QObject::connect(&shutdown_timer, &QTimer::timeout, [this, delay, time_elapsed]() mutable {
            time_remaining = delay - time_elapsed;

            if (time_remaining <= std::chrono::minutes(5) ||
                time_remaining % std::chrono::minutes(5) == std::chrono::minutes::zero())
            {
                write_shutdown_message(*virtual_machine, time_remaining);
            }

            if (time_elapsed >= delay)
            {
                shutdown_timer.stop();
                shutdown_instance();
            }
            else
            {
                time_elapsed += std::chrono::minutes(1);
            }
        });

        virtual_machine->state = VirtualMachine::State::delayed_shutdown;

        shutdown_timer.start(delay < std::chrono::minutes(1) ? delay : std::chrono::minutes(1));
    }
    else
    {
        write_shutdown_message(*virtual_machine, std::chrono::milliseconds::zero());
        shutdown_instance();
    }
}

std::chrono::seconds mp::DelayedShutdownTimer::get_time_remaining()
{
    return std::chrono::duration_cast<std::chrono::minutes>(time_remaining);
}

void mp::DelayedShutdownTimer::shutdown_instance()
{
    stop_mounts(virtual_machine->vm_name);
    virtual_machine->shutdown();
    emit finished();
}
