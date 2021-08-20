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

struct Timer : public testing::Test
{
    std::atomic_bool timedout{false}, test_notify_called{false}, timer_running{false}, timer_paused{false};
    std::condition_variable cv;
    std::mutex cv_m;
    const std::chrono::milliseconds default_timeout{10};
    const std::chrono::seconds default_wait{1};

    MockTimerSyncFuncs::GuardedMock attr{MockTimerSyncFuncs::inject()};
    MockTimerSyncFuncs* mock_timer_sync_funcs = attr.first;
};
} // namespace

TEST_F(Timer, times_out)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Unlock the Timer's internal lock
                lock.unlock();

                // Wait for notification from main test thread
                cv.wait(lk, [this] { return test_notify_called.load(); });

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return std::cv_status::timeout;
            });

    // notify_all is called twice via the stop() in start() and the stop() in the dtor
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_)).Times(2).WillRepeatedly(Return());

    mpu::Timer t{default_timeout, [this]()
                 {
                     {
                         std::lock_guard<std::mutex> lk{cv_m};
                         timedout.store(true);
                     }
                     cv.notify_all();
                 }};

    t.start();

    std::unique_lock<std::mutex> lk(cv_m);
    test_notify_called.store(true);

    // Notify the mocked wait_for()
    cv.notify_all();

    ASSERT_TRUE(cv.wait_for(lk, default_wait, [this] { return timedout.load(); }))
        << "Did not time out in reasonable timeframe";
}

TEST_F(Timer, stops)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Unlock the Timer's internal lock
                lock.unlock();

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return std::cv_status::no_timeout;
            });

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .WillOnce(Return()) // Handles the stop() in start()
        .WillOnce(          // Handles the stop() called in the test
            [this](auto&)
            {
                {
                    std::unique_lock<std::mutex> lk(cv_m);
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
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    // stop() blocks until the Timer thread has joined, ie, it is finished
    t.stop();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, pauses)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Unlock the Timer's internal lock
                lock.unlock();

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return std::cv_status::no_timeout;
            });

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(3)
        .WillOnce(Return()) // Handles the stop() in start()
        .WillRepeatedly(    // Handles the other 2 notify_all() calls
            [this](auto&)
            {
                {
                    std::unique_lock<std::mutex> lk(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait() and wait_for()
                cv.notify_all();
            });

    EXPECT_CALL(*mock_timer_sync_funcs, wait(_, _))
        .WillOnce(
            [this](auto&, auto& lock)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Unlock the Timer's internal lock
                lock.unlock();

                timer_paused.store(true);
                cv.notify_all();

                // Waits until the Timer dtor calls stop()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();
            });

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    t.pause();

    {
        // Wait for the the timer to be "paused" in the mocked wait()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_paused.load(); });
    }

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, resumes)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Unlock the Timer's internal lock
                lock.unlock();

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return std::cv_status::no_timeout;
            })
        .WillOnce([](auto&...) { return std::cv_status::timeout; });

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(4)
        .WillOnce(Return())             // For the stop() in start()
        .WillRepeatedly([this](auto&) { // Handles the 3 other notify_all() calls
            {
                std::unique_lock<std::mutex> lk(cv_m);
                test_notify_called.store(true);
            }
            // Notify the mocked wait() and wait_for()
            cv.notify_all();
        });

    EXPECT_CALL(*mock_timer_sync_funcs, wait(_, _))
        .WillOnce(
            [this](auto&, auto& lock)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                timer_paused.store(true);
                cv.notify_all();

                // Unlock the Timer's internal lock
                lock.unlock();

                // Waits until resume is called()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();
            });

    mpu::Timer t{default_timeout, [this]()
                 {
                     {
                         std::lock_guard<std::mutex> lk{cv_m};
                         timedout.store(true);
                     }
                     cv.notify_all();
                 }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    t.pause();

    {
        // Wait for the the timer to be "paused" in the mocked wait()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_paused.load(); });
    }

    t.resume();

    std::unique_lock<std::mutex> lk(cv_m);
    // Wait on the actual Timer timeout
    ASSERT_TRUE(cv.wait_for(lk, default_wait, [this] { return timedout.load(); }))
        << "Did not time out in reasonable timeframe";
}

TEST_F(Timer, stops_paused)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Unlock the Timer's internal lock
                lock.unlock();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return std::cv_status::no_timeout;
            });

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(4)
        .WillOnce(Return()) // For the stop() in start()
        .WillRepeatedly(
            [this](auto&)
            {
                {
                    std::unique_lock<std::mutex> lk(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait() and wait_for()
                cv.notify_all();
            });

    EXPECT_CALL(*mock_timer_sync_funcs, wait(_, _))
        .WillOnce(
            [this](auto&, auto& lock)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                timer_paused.store(true);
                cv.notify_all();

                // Unlock the Timer's internal lock
                lock.unlock();

                // Waits until stop() is called
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();
            });

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    t.pause();

    {
        // Wait for the the timer to be "paused" in the mocked wait()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_paused.load(); });
    }

    t.stop();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, cancels)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Unlock the Timer's internal lock
                lock.unlock();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });

                return std::cv_status::no_timeout;
            });

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .WillOnce(Return()) // For the stop() in start()
        .WillOnce(
            [this](auto&)
            {
                {
                    std::unique_lock<std::mutex> lk(cv_m);
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
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait(lk, [this] { return timer_running.load(); });
        }
    }

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, restarts)
{
    std::atomic_int count = 0;
    std::cv_status timeout_status{std::cv_status::no_timeout};

    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .Times(2)
        .WillRepeatedly(
            [this, &timeout_status](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Unlock the Timer's internal lock
                lock.unlock();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return timeout_status;
            });

    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(3)
        .WillOnce(Return()) // For the stop() in start()
        .WillRepeatedly(
            [this](auto&)
            {
                {
                    std::unique_lock<std::mutex> lk(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait() and wait_for()
                cv.notify_all();
            });

    mpu::Timer t{default_timeout, [this, &count]()
                 {
                     {
                         std::lock_guard<std::mutex> lk{cv_m};
                         ++count;
                     }
                     cv.notify_all();
                 }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    t.start();

    {
        // Wait for the the timer to be "running" again in the mocked wait_for()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    ASSERT_EQ(count.load(), 0) << "Should not have timed out yet";

    std::unique_lock<std::mutex> lk(cv_m);
    timeout_status = std::cv_status::timeout;
    test_notify_called.store(true);

    // Notify the mocked wait_for()
    cv.notify_all();

    // Wait on the Timer callback
    ASSERT_TRUE(cv.wait_for(lk, default_wait, [&count] { return count.load() > 0; }))
        << "Did not time out in reasonable timeframe";

    ASSERT_EQ(count.load(), 1) << "Should have timed out once now";
}

TEST_F(Timer, stopped_ignores_pause)
{
    // Indicates the Timer was never running
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _)).Times(0);

    // Indicates that pause() just returns and notify_all() is only called in stop() that the Timer dtor calls
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_)).WillOnce(Return());

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.pause();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, stopped_ignores_resume)
{
    // Indicates the Timer was never running
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _)).Times(0);

    // Indicates that resume() just returns and notify_all() is only called in stop() that the Timer dtor calls
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_)).WillOnce(Return());

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.resume();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, running_ignores_resume)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, _))
        .WillOnce(
            [this](auto&, auto& lock, auto&)
            {
                std::unique_lock<std::mutex> lk(cv_m);

                // Unlock the Timer's internal lock
                lock.unlock();

                // Notify the test thread that the timer is running
                timer_running.store(true);
                cv.notify_all();

                // Wait to be notified by the mock notify_all()
                cv.wait(lk, [this] { return test_notify_called.load(); });
                test_notify_called.store(false);

                // Re-lock the Timer's internal lock before returning
                lock.lock();

                return std::cv_status::no_timeout;
            });

    // notify_all() should only be called in the stop() that start() calls and in the Timer dtor
    // but not in resume()
    EXPECT_CALL(*mock_timer_sync_funcs, notify_all(_))
        .Times(2)
        .WillOnce(Return()) // Handles the stop() in start()
        .WillOnce(          // Handles the notify_all() call in the dtor
            [this](auto&)
            {
                {
                    std::unique_lock<std::mutex> lk(cv_m);
                    test_notify_called.store(true);
                }
                // Notify the mocked wait_for()
                cv.notify_all();
            });

    mpu::Timer t{default_timeout, [this]() { timedout.store(true); }};

    t.start();

    {
        // Wait for the the timer to be "running" in the mocked wait_for()
        std::unique_lock<std::mutex> lk(cv_m);
        cv.wait(lk, [this] { return timer_running.load(); });
    }

    t.resume();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}
