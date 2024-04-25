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

#ifndef MULTIPASS_MOCK_SSH_PROCESS_EXIT_STATUS
#define MULTIPASS_MOCK_SSH_PROCESS_EXIT_STATUS

#include "mock_ssh.h"

#include <optional>

namespace multipass::test
{
class ExitStatusMock
{
public:
    ExitStatusMock()
    {
        add_channel_cbs = [this](ssh_channel, ssh_channel_callbacks cb) {
            channel_cbs = cb;
            return SSH_OK;
        };

        event_do_poll = [this](auto...) {
            if (channel_cbs == nullptr)
                return SSH_ERROR;

            if (exit_code)
                channel_cbs->channel_exit_status_function(nullptr, nullptr, *exit_code, channel_cbs->userdata);

            return ssh_rc;
        };
    }

    ~ExitStatusMock()
    {
        add_channel_cbs = std::move(old_add_channel_cbs);
        event_do_poll = std::move(old_event_do_poll);
    }

    void set_ssh_rc(int rc)
    {
        ssh_rc = rc;
    }

    void set_no_exit()
    {
        exit_code.reset();
    }

    void set_exit_status(int code)
    {
        exit_code = code;
    }

    static constexpr int success_status = 0;
    static constexpr int failure_status = 42;

private:
    decltype(mock_ssh_add_channel_callbacks)& add_channel_cbs{mock_ssh_add_channel_callbacks};
    decltype(mock_ssh_add_channel_callbacks) old_add_channel_cbs{std::move(mock_ssh_add_channel_callbacks)};
    decltype(mock_ssh_event_dopoll)& event_do_poll{mock_ssh_event_dopoll};
    decltype(mock_ssh_event_dopoll) old_event_do_poll{std::move(mock_ssh_event_dopoll)};

    int ssh_rc{SSH_OK};
    std::optional<int> exit_code{0};
    ssh_channel_callbacks channel_cbs{nullptr};
};
} // namespace multipass::test
#endif // MULTIPASS_MOCK_SSH_PROCESS_EXIT_STATUS
