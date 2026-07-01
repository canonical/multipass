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

#include "mock_ssh.h"

#include <optional>
#include <queue>

namespace multipass::test
{
struct CallbackState
{
    int ssh_rc{SSH_OK};
    bool eof{true};
    bool closed{true};
    std::optional<int> exit_code{0};
};

class CallbackEngineMock // TODO@rewiressh remove (and can we can rid of premock entirely?)
{
public:
    CallbackEngineMock()
    {
        add_channel_cbs = [this](ssh_channel, ssh_channel_callbacks cb) {
            channel_cbs = cb;
            return SSH_OK;
        };

        event_do_poll = [this](auto...) {
            // Explicit copy
            CallbackState cb_s{cb_state.front()};

            this->pop_state();

            if (channel_cbs == nullptr)
                return SSH_ERROR;

            if (cb_s.exit_code)
                channel_cbs->channel_exit_status_function(nullptr,
                                                          nullptr,
                                                          *cb_s.exit_code,
                                                          channel_cbs->userdata);
            if (cb_s.eof)
                channel_cbs->channel_eof_function(nullptr, nullptr, channel_cbs->userdata);

            if (cb_s.closed)
                channel_cbs->channel_close_function(nullptr, nullptr, channel_cbs->userdata);

            return cb_s.ssh_rc;
        };

        remove_channel_cbs = [](auto...) { return SSH_OK; };
        cb_state.push(process_exit_success);
    }

    ~CallbackEngineMock()
    {
        add_channel_cbs = std::move(old_add_channel_cbs);
        event_do_poll = std::move(old_event_do_poll);
        remove_channel_cbs = std::move(old_remove_channel_cbs);
    }

    void push_state(CallbackState cb_s)
    {
        cb_state.push(cb_s);
    }

    void pop_state()
    {
        if (cb_state.size() > 1)
            cb_state.pop();
    }

    static constexpr int success_code = 0;
    static constexpr int failure_code = 42;

    static constexpr CallbackState process_exit_success{SSH_OK, true, true, success_code};
    static constexpr CallbackState process_exit_failure{SSH_OK, true, true, failure_code};
    static constexpr CallbackState process_noexit{SSH_ERROR, true, true, std::nullopt};
    static constexpr CallbackState process_running{SSH_AGAIN, false, false, std::nullopt};

private:
    decltype(mock_ssh_add_channel_callbacks)& add_channel_cbs{mock_ssh_add_channel_callbacks};
    decltype(mock_ssh_add_channel_callbacks) old_add_channel_cbs{
        std::move(mock_ssh_add_channel_callbacks)};
    decltype(mock_ssh_event_dopoll)& event_do_poll{mock_ssh_event_dopoll};
    decltype(mock_ssh_event_dopoll) old_event_do_poll{std::move(mock_ssh_event_dopoll)};
    decltype(mock_ssh_remove_channel_callbacks)& remove_channel_cbs{
        mock_ssh_remove_channel_callbacks};
    decltype(mock_ssh_remove_channel_callbacks) old_remove_channel_cbs{
        std::move(mock_ssh_remove_channel_callbacks)};

    ssh_channel_callbacks channel_cbs{nullptr};
    // By default it behaves like the previous implementation
    std::queue<CallbackState> cb_state{};
};
} // namespace multipass::test
