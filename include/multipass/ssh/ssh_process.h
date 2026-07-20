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

#pragma once

#include <multipass/disabled_copy_move.h>

#include <chrono>
#include <string>

namespace sch = std::chrono;

namespace multipass
{
class SSHProcess : private DisabledCopyMove
{
public:
    virtual ~SSHProcess() = default;

    /**
     * Obtain the exit code of the process, blocking up to the given timeout.
     * @param timeout Maximum time to wait for the exit code.
     * @return Optional with the return code if it was not obtained.
     * @throws ExitlessSSHProcessException (or descendant) if the exit code cannot be obtained
     * within the timeout.
     */
    virtual int get_exit_code(sch::milliseconds timeout = sch::seconds(5)) = 0;

    virtual std::string read_std_output(sch::milliseconds timeout = sch::seconds(1000)) = 0;
    virtual std::string read_std_error(sch::milliseconds timeout = sch::seconds(1000)) = 0;
    virtual const std::string& get_cmd() const = 0;

    /**
     * Obtain a boolean that indicates whether the object will receive further data or not. Exit
     * code is not enough to ensure all output has been received.
     * @param timeout Maximum time to wait for the boolean.
     * @throws SSHProcessExitException if an event was not allocated properly or if it could not be
     * added to the session
     * @return The boolean.
     */
    virtual bool is_terminated(sch::milliseconds timeout = sch::milliseconds(10)) = 0;

    /**
     * Stop the process gracefully
     */
    virtual void close() = 0;

protected:
    SSHProcess() = default;
};
} // namespace multipass
