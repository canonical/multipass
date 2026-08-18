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

#include "mock_libssh.h"

#include <chrono>
#include <optional>
#include <queue>
#include <string>
#include <thread>

namespace multipass::test
{
struct CallbackChState
{
    int ssh_rc{SSH_OK};
    bool eof{true};
    bool closed{true};
    std::optional<int> exit_code{0};
    std::optional<std::string> signal;
    std::chrono::milliseconds wait{std::chrono::milliseconds(0)};
};

class CallbackChEngineMock // TODO@rewiressh remove (and can we can rid of premock entirely?)
{
public:
    CallbackChEngineMock(MockLibssh& mock_libssh, CallbackChState initial_state)
    {
        cb_state.push(initial_state);
        ON_CALL(mock_libssh, ssh_add_channel_callbacks)
            .WillByDefault([this](ssh_channel, ssh_channel_callbacks cb) {
                channel_cbs = cb;
                return SSH_OK;
            });

        ON_CALL(mock_libssh, ssh_event_dopoll).WillByDefault([this](auto...) {
            // Explicit copy
            CallbackChState cb_s{cb_state.front()};

            this->pop_state();
            std::this_thread::sleep_for(cb_s.wait);

            if (channel_cbs == nullptr)
                return SSH_ERROR;

            if (cb_s.exit_code)
                channel_cbs->channel_exit_status_function(nullptr,
                                                          nullptr,
                                                          *cb_s.exit_code,
                                                          channel_cbs->userdata);
            if (cb_s.signal)
                channel_cbs->channel_exit_signal_function(nullptr,
                                                          nullptr,
                                                          cb_s.signal->c_str(),
                                                          0,
                                                          nullptr,
                                                          nullptr,
                                                          channel_cbs->userdata);
            if (cb_s.eof)
                channel_cbs->channel_eof_function(nullptr, nullptr, channel_cbs->userdata);

            if (cb_s.closed)
                channel_cbs->channel_close_function(nullptr, nullptr, channel_cbs->userdata);

            return cb_s.ssh_rc;
        });

        ON_CALL(mock_libssh, ssh_remove_channel_callbacks)
            .WillByDefault([this](ssh_channel, ssh_channel_callbacks cb) {
                if (cb == channel_cbs)
                {
                    channel_cbs = nullptr;
                    return SSH_OK;
                }
                return SSH_ERROR;
            });
    }

    ~CallbackChEngineMock()
    {
    }

    void push_state(CallbackChState cb_s)
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

    static constexpr CallbackChState channel_exit_success{SSH_OK,
                                                          true,
                                                          true,
                                                          success_code,
                                                          std::nullopt,
                                                          std::chrono::milliseconds(0)};
    static constexpr CallbackChState channel_exit_failure{SSH_OK,
                                                          true,
                                                          true,
                                                          failure_code,
                                                          std::nullopt,
                                                          std::chrono::milliseconds(0)};
    static constexpr CallbackChState channel_sigterm_exit{SSH_OK,
                                                          true,
                                                          true,
                                                          failure_code,
                                                          "TERM",
                                                          std::chrono::milliseconds(0)};
    static constexpr CallbackChState channel_noexit{SSH_ERROR,
                                                    true,
                                                    true,
                                                    std::nullopt,
                                                    std::nullopt,
                                                    std::chrono::milliseconds(0)};
    static constexpr CallbackChState channel_running{SSH_AGAIN,
                                                     false,
                                                     false,
                                                     std::nullopt,
                                                     std::nullopt,
                                                     std::chrono::milliseconds(50)};
    static constexpr CallbackChState channel_timeout{SSH_AGAIN,
                                                     false,
                                                     false,
                                                     std::nullopt,
                                                     std::nullopt,
                                                     std::chrono::milliseconds(250)};

private:
    ssh_channel_callbacks channel_cbs{nullptr};
    // By default it behaves like the previous implementation
    std::queue<CallbackChState> cb_state{};
};
} // namespace multipass::test
