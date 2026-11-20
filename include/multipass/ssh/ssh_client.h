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

#include <multipass/cli/return_codes.h>
#include <multipass/disabled_copy_move.h>

#include <string>
#include <vector>

namespace multipass
{
class SSHClient : multipass::DisabledCopyMove
{
public:
    virtual ~SSHClient() = default;

    /**
     * Execute a list of commands in the remote ssh session
     * @param args_list List of command argument lists to execute
     * @return The return code resulting from the remote session executing the commands
     */
    virtual multipass::VMReturnCode exec(
        const std::vector<std::vector<std::string>>& args_list) = 0;
    /**
     * Open a shell in the remote ssh session
     * @return The last return code from the shell
     */
    virtual multipass::VMReturnCode open_shell() = 0;

protected:
    SSHClient() = default;
};
} // namespace multipass
