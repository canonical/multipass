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
#include <multipass/logging/log.h>

#include <multipass/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
void write_shutdown_message(std::optional<mp::SSHSession>& ssh_session, const std::chrono::minutes& time_left,
                            const std::string& name)
{
    if (ssh_session)
    {
        if (time_left > std::chrono::milliseconds::zero())
        {
            ssh_session->exec(
                fmt::format("wall \"The system is going down for poweroff in {} minute{}, use 'multipass stop "
                            "--cancel {}' to cancel the shutdown.\"",
                            time_left.count(), time_left > std::chrono::minutes(1) ? "s" : "", name));
        }
        else
        {
            ssh_session->exec(fmt::format("wall The system is going down for poweroff now"));
        }
    }
}
} // namespace

mp::DelayedShutdownTimer::DelayedShutdownTimer(VirtualMachine* virtual_machine, std::optional<SSHSession>&& session,
                                               const StopMounts& stop_mounts)
    : virtual_machine{virtual_machine}, ssh_session{std::move(session)}, stop_mounts{stop_mounts}
{
}

mp::DelayedShutdownTimer::~DelayedShutdownTimer()
{
    if (shutdown_timer.isActive())
    {
        try
        {
            if (ssh_session)
            {
                // exit_code() is here to make sure the command finishes before continuing in the dtor
                ssh_session->exec("wall The system shutdown has been cancelled").exit_code();
            }
            mpl::log(mpl::Level::info, virtual_machine->vm_name, fmt::format("Cancelling delayed shutdown"));
            virtual_machine->state = VirtualMachine::State::running;
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::warning, virtual_machine->vm_name,
                     fmt::format("Unable to cancel delayed shutdown: {}", e.what()));
            virtual_machine->state = VirtualMachine::State::unknown;
        }
    }
}

void mp::DelayedShutdownTimer::start(const std::chrono::milliseconds delay)
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
        write_shutdown_message(ssh_session, std::chrono::duration_cast<std::chrono::minutes>(delay),
                               virtual_machine->vm_name);

        time_remaining = delay;
        std::chrono::minutes time_elapsed{1};
        QObject::connect(&shutdown_timer, &QTimer::timeout, [this, delay, time_elapsed]() mutable {
            time_remaining = delay - time_elapsed;

            if (time_remaining <= std::chrono::minutes(5) ||
                time_remaining % std::chrono::minutes(5) == std::chrono::minutes::zero())
            {
                write_shutdown_message(ssh_session, std::chrono::duration_cast<std::chrono::minutes>(time_remaining),
                                       virtual_machine->vm_name);
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
        write_shutdown_message(ssh_session, std::chrono::minutes::zero(), virtual_machine->vm_name);
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
