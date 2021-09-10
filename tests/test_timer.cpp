/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "common.h"
#include "mock_singleton_helpers.h"

#include <multipass/timer.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace mp = multipass;
namespace mpu = multipass::utils;

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace testing;

namespace
{
struct MockTimerSyncFuncs : public mpu::TimerSyncFuncs
{
    using TimerSyncFuncs::TimerSyncFuncs;

    MOCK_METHOD1(notify_all, void(std::condition_variable&));
    MOCK_METHOD2(wait, void(std::condition_variable&, std::unique_lock<std::mutex>&));
    MOCK_METHOD3(wait_for, std::cv_status(std::condition_variable&, std::unique_lock<std::mutex>&,
                                          const std::chrono::duration<int, std::milli>&));

    MP_MOCK_SINGLETON_BOILERPLATE(MockTimerSyncFuncs, TimerSyncFuncs);
};

struct TestTimer : public testing::Test
{
    auto mock_wait(std::unique_lock<std::mutex>& timer_lock, std::atomic_bool& target_state)
    {
        std::unique_lock<std::mutex> test_lock(cv_m);

        // Unlock the Timer's internal lock
        timer_lock.unlock();

        // Notify the test thread that the timer is running
        target_state.store(true);
        cv.notify_all();

        // Wait for notification from main test thread
        cv.wait(test_lock, [this] { return test_notify_called.load(); });
        test_notify_called.store(false);

        // Re-lock the Timer's internal lock before returning
        timer_lock.lock();
    }

    auto make_mock_wait_for(std::cv_status timeout_status)
    {
        return [this, timeout_status](std::unique_lock<std::mutex>& timer_lock) {
            mock_wait(timer_lock, timer_running);
            return timeout_status;
        };
    }

    void SetUp() override
    {
        ON_CALL(*mock_timer_sync_funcs, wait).WillByDefault(WithArg<1>([this](auto& timer_lock) {
            mock_wait(timer_lock, timer_paused);
        }));
    }

    std::atomic_bool timedout{false}, test_notify_called{false}, timer_running{false}, timer_paused{false};
    std::condition_variable cv;
    std::mutex cv_m;
    const std::chrono::milliseconds default_timeout{10};
    const std::chrono::seconds default_wait{1};

    MockTimerSyncFuncs::GuardedMock attr{MockTimerSyncFuncs::inject()};
    MockTimerSyncFuncs* mock_timer_sync_funcs = attr.first;
};

} // namespace

TEST_F(TestTimer, times_out)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::timeout)));

    // notify_all is called twice via the stop() in start() and the stop() in the dtor
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_)).Times(2).WillRepeatedly(Return());

    mpu::Timer t{default_timeout, [this]() {
                     {
                         std::lock_guard<std::mutex> test_lock{cv_m};
                         timedout.store(true);
                     }
                     cv.notify_all();
                 }};

    t.start();

    std::unique_lock<std::mutex> test_lock(cv_m);
    test_notify_called.store(true);

    // Notify the mocked wait_for()
    cv.notify_all();

    ASSERT_TRUE(cv.wait_for(test_lock, default_wait, [this] { return timedout.load(); }))
        << "Did not time out in reasonable timeframe";
}

TEST_F(TestTimer, stops)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)));

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .WillOnce(Return()) // Handles the stop() in start()
        .WillOnce(          // Handles the stop() called in the test
            [this](auto&) {
                {
                    std::unique_lock<std::mutex> test_lock(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait_for()
                cv.notify_all();
            })
        .WillOnce(Return()); // Handles the stop() in the Timer dtor

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    // stop() blocks until the Timer thread has joined, ie, it is finished
    t.stop();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(TestTimer, pauses)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)));

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(3)
        .WillOnce(Return()) // Handles the stop() in start()
        .WillRepeatedly(    // Handles the other 2 notify_all() calls
            [this](auto&) {
                {
                    std::unique_lock<std::mutex> test_lock(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait() and wait_for()
                cv.notify_all();
            });

    EXPECT_CALL(*mock_timer_sync_funcs, wait);

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    t.pause();

    {
        // Wait for the the timer to be "paused" in the mocked wait()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_paused.load(); });
    }

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(TestTimer, resumes)
{
    // The initial start
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)));
    // After resume() is called, there should be less time left than the default timeout
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Lt(default_timeout))).WillOnce(Return(std::cv_status::timeout));

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(4)
        .WillOnce(Return())             // For the stop() in start()
        .WillRepeatedly([this](auto&) { // Handles the 3 other notify_all() calls
            {
                std::unique_lock<std::mutex> test_lock(cv_m);
                test_notify_called.store(true);
            }
            // Notify the mocked wait() and wait_for()
            cv.notify_all();
        });

    EXPECT_CALL(*mock_timer_sync_funcs, wait);

    mpu::Timer t{default_timeout, [this]() {
                     {
                         std::lock_guard<std::mutex> test_lock{cv_m};
                         timedout.store(true);
                     }
                     cv.notify_all();
                 }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    t.pause();

    {
        // Wait for the the timer to be "paused" in the mocked wait()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_paused.load(); });
    }

    t.resume();

    std::unique_lock<std::mutex> test_lock(cv_m);
    // Wait on the actual Timer timeout
    ASSERT_TRUE(cv.wait_for(test_lock, default_wait, [this] { return timedout.load(); }))
        << "Did not time out in reasonable timeframe";
}

TEST_F(TestTimer, stops_paused)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)));

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(4)
        .WillOnce(Return()) // For the stop() in start()
        .WillRepeatedly([this](auto&) {
            {
                std::unique_lock<std::mutex> test_lock(cv_m);
                test_notify_called.store(true);
            }
            // Notify the mocked wait() and wait_for()
            cv.notify_all();
        });

    EXPECT_CALL(*mock_timer_sync_funcs, wait);

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    t.pause();

    {
        // Wait for the the timer to be "paused" in the mocked wait()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_paused.load(); });
    }

    t.stop();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(TestTimer, cancels)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)));

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .WillOnce(Return()) // For the stop() in start()
        .WillOnce([this](auto&) {
            {
                std::unique_lock<std::mutex> test_lock(cv_m);
                test_notify_called.store(true);
            }
            // Notify the mocked wait_for()
            cv.notify_all();
        });

    {
        mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};
        t.start();

        {
            // Wait for the the timer to be "running" in the mocked wait_for()
            std::unique_lock<std::mutex> test_lock(cv_m);
            cv.wait(test_lock, [this] { return timer_running.load(); });
        }
    }

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(TestTimer, restarts)
{
    std::atomic_int count = 0;

    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::timeout)));

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(3)
        .WillOnce(Return()) // For the stop() in start()
        .WillRepeatedly([this](auto&) {
            {
                std::unique_lock<std::mutex> test_lock(cv_m);
                test_notify_called.store(true);
            }
            // Notify the mocked wait() and wait_for()
            cv.notify_all();
        });

    mpu::Timer t{default_timeout, [this, &count]() {
                     {
                         std::lock_guard<std::mutex> test_lock{cv_m};
                         ++count;
                     }
                     cv.notify_all();
                 }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    t.start();

    {
        // Wait for the the timer to be "running" again in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    ASSERT_EQ(count.load(), 0) << "Should not have timed out yet";

    std::unique_lock<std::mutex> test_lock(cv_m);
    test_notify_called.store(true);

    // Notify the mocked wait_for()
    cv.notify_all();

    // Wait on the Timer callback
    ASSERT_TRUE(cv.wait_for(test_lock, default_wait, [&count] { return count.load() > 0; }))
        << "Did not time out in reasonable timeframe";

    ASSERT_EQ(count.load(), 1) << "Should have timed out once now";
}

TEST_F(TestTimer, stopped_ignores_pause)
{
    // Indicates the Timer was never running
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _)).Times(0);

    // Indicates that pause() just returns and notify_all() is only called in stop() that the Timer dtor calls
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_)).WillOnce(Return());

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.pause();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(TestTimer, stopped_ignores_resume)
{
    // Indicates the Timer was never running
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _)).Times(0);

    // Indicates that resume() just returns and notify_all() is only called in stop() that the Timer dtor calls
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_)).WillOnce(Return());

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.resume();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(TestTimer, running_ignores_resume)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout)))
        .WillOnce(WithArg<1>(make_mock_wait_for(std::cv_status::no_timeout)));

    // notify_all() should only be called in the stop() that start() calls and in the Timer dtor
    // but not in resume()
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(2)
        .WillOnce(Return()) // Handles the stop() in start()
        .WillOnce(          // Handles the notify_all() call in the dtor
            [this](auto&) {
                {
                    std::unique_lock<std::mutex> test_lock(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait_for()
                cv.notify_all();
            });

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [this] { return timer_running.load(); });
    }

    t.resume();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}
