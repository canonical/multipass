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

#include "sleep_wake_handler.h"

#include <multipass/logging/log.h>

#include <AppKit/NSWorkspace.h>
#include <Foundation/NSNotification.h>
#include <Foundation/NSString.h>

namespace mpl = multipass::logging;

namespace
{
constexpr auto log_category = "sleep-wake-handler";
} // namespace

class multipass::platform::macos::SleepWakeHandler::Impl
{
public:
    SuspendCallback suspend_callback;
    ResumeCallback resume_callback;
    bool enabled = true;
    id sleep_observer = nil;
    id wake_observer = nil;

    Impl()
    {
        setup_notifications();
    }

    ~Impl()
    {
        remove_observers();
    }

    void setup_notifications()
    {
        auto* center = [NSWorkspace sharedWorkspace].notificationCenter;

        // Register for sleep notification
        sleep_observer = [center addObserverForName:NSWorkspaceWillSleepNotification
                                              object:nil
                                               queue:nil
                                          usingBlock:^(NSNotification* _Nonnull note) {
                                              handle_sleep_notification();
                                          }];

        // Register for wake notification
        wake_observer = [center addObserverForName:NSWorkspaceDidWakeNotification
                                            object:nil
                                             queue:nil
                                        usingBlock:^(NSNotification* _Nonnull note) {
                                            handle_wake_notification();
                                        }];

        mpl::trace(log_category, "Sleep/wake notifications registered");
    }

    void remove_observers()
    {
        auto* center = [NSWorkspace sharedWorkspace].notificationCenter;
        if (sleep_observer)
        {
            [center removeObserver:sleep_observer];
            sleep_observer = nil;
        }
        if (wake_observer)
        {
            [center removeObserver:wake_observer];
            wake_observer = nil;
        }
        mpl::trace(log_category, "Sleep/wake notifications removed");
    }

    void handle_sleep_notification()
    {
        if (!enabled)
        {
            mpl::debug(log_category, "Sleep notification ignored (handler disabled)");
            return;
        }

        mpl::info(log_category, "System is about to sleep");

        if (suspend_callback)
        {
            try
            {
                suspend_callback();
            }
            catch (const std::exception& e)
            {
                mpl::error(log_category, "Error in suspend callback: {}", e.what());
            }
        }
    }

    void handle_wake_notification()
    {
        if (!enabled)
        {
            mpl::debug(log_category, "Wake notification ignored (handler disabled)");
            return;
        }

        mpl::info(log_category, "System woke up from sleep");

        if (resume_callback)
        {
            try
            {
                resume_callback();
            }
            catch (const std::exception& e)
            {
                mpl::error(log_category, "Error in resume callback: {}", e.what());
            }
        }
    }
};

multipass::platform::macos::SleepWakeHandler::SleepWakeHandler()
    : impl{std::make_unique<Impl>()}
{
    mpl::debug(log_category, "SleepWakeHandler created");
}

multipass::platform::macos::SleepWakeHandler::~SleepWakeHandler()
{
    mpl::debug(log_category, "SleepWakeHandler destroyed");
}

void multipass::platform::macos::SleepWakeHandler::set_suspend_callback(SuspendCallback callback)
{
    impl->suspend_callback = std::move(callback);
}

void multipass::platform::macos::SleepWakeHandler::set_resume_callback(ResumeCallback callback)
{
    impl->resume_callback = std::move(callback);
}

void multipass::platform::macos::SleepWakeHandler::set_enabled(bool enabled)
{
    impl->enabled = enabled;
    mpl::debug(log_category, "SleepWakeHandler {}", enabled ? "enabled" : "disabled");
}

bool multipass::platform::macos::SleepWakeHandler::is_enabled() const
{
    return impl->enabled;
}
