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

#include <multipass/ssh/plain_ssh_client.h>
#include <multipass/ssh/plain_ssh_session.h>
#include <multipass/terminal.h>

#include <memory>

namespace multipass
{
class Terminal;
class PlatformSSHClient : public PlainSSHClient
{
public:
    PlatformSSHClient(multipass::PlainSSHSession&& ssh_session_obj, Terminal* term);
    ~PlatformSSHClient() override;

    struct PlatformData;

    struct ConsoleGeometry
    {
        int rows;
        int columns;
    };

protected:
    void handle_ssh_events(ssh_session session) override;

private:
    void restore_console();
    void setup_console();

    Terminal* term;
    std::unique_ptr<PlatformData> p_data;
    bool is_interactive;
};
} // namespace multipass
