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

#ifndef MULTIPASS_DELAYED_SHUTDOWN_TIMER_H
#define MULTIPASS_DELAYED_SHUTDOWN_TIMER_H

#include <multipass/disabled_copy_move.h>
#include <multipass/virtual_machine.h>

#include <QObject>
#include <QTimer>

#include <chrono>
#include <functional>
#include <string>

namespace multipass
{
class DelayedShutdownTimer : public QObject, private DisabledCopyMove
{
    Q_OBJECT

public:
    using StopMounts = std::function<void(const std::string&)>;
    DelayedShutdownTimer(VirtualMachine* virtual_machine, const StopMounts& stop_mounts);
    ~DelayedShutdownTimer();

    void start(const std::chrono::milliseconds delay);
    std::chrono::seconds get_time_remaining();

signals:
    void finished();

private:
    void shutdown_instance();

    QTimer shutdown_timer;
    VirtualMachine* virtual_machine;
    const StopMounts stop_mounts;
    std::chrono::milliseconds time_remaining;
};
} // namespace multipass
#endif // MULTIPASS_DELAYED_SHUTDOWN_TIMER_H
