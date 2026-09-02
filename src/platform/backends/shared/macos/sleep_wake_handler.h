/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include <functional>
#include <memory>

namespace multipass::platform::macos
{
/**
 * @brief SleepWakeHandler handles macOS system sleep and wake notifications.
 *
 * This class subscribes to NSWorkspaceWillSleepNotification and
 * NSWorkspaceDidWakeNotification to automatically suspend and resume
 * Multipass instances when the system goes to sleep or wakes up.
 */
class SleepWakeHandler
{
public:
    using SuspendCallback = std::function<void()>;
    using ResumeCallback = std::function<void()>;

    SleepWakeHandler();
    ~SleepWakeHandler();

    // Disable copy and move
    SleepWakeHandler(const SleepWakeHandler&) = delete;
    SleepWakeHandler& operator=(const SleepWakeHandler&) = delete;
    SleepWakeHandler(SleepWakeHandler&&) = delete;
    SleepWakeHandler& operator=(SleepWakeHandler&&) = delete;

    /**
     * @brief Set the callback to be invoked when system is about to sleep.
     * @param callback The callback function to invoke.
     */
    void set_suspend_callback(SuspendCallback callback);

    /**
     * @brief Set the callback to be invoked when system wakes up.
     * @param callback The callback function to invoke.
     */
    void set_resume_callback(ResumeCallback callback);

    /**
     * @brief Enable or disable the sleep/wake handler.
     * @param enabled True to enable, false to disable.
     */
    void set_enabled(bool enabled);

    /**
     * @brief Check if the handler is enabled.
     * @return True if enabled, false otherwise.
     */
    bool is_enabled() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace multipass::platform::macos
