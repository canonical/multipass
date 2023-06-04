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

    MOCK_METHOD(void, notify_all, (std::condition_variable&), (const, override));
    MOCK_METHOD(void, wait, (std::condition_variable&, std::unique_lock<std::mutex>&, std::function<bool()>),
                (const, override));
    MOCK_METHOD(bool, wait_for,
                (std::condition_variable&, std::unique_lock<std::mutex>&,
                 (const std::chrono::duration<int, std::milli>&), std::function<bool()>),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockTimerSyncFuncs, TimerSyncFuncs);
};

struct TestTimer : public testing::Test
{
    TestTimer()
        : t{default_timeout, [this] {
                {
                    std::lock_guard<std::mutex> test_lock{cv_m};
                    ++timeout_count;
                }
                cv.notify_all();
            }}
    {
    }

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

    auto make_mock_wait_for(const bool& timeout_status)
    {
        return [this, timeout_status](std::unique_lock<std::mutex>& timer_lock) {
            mock_wait(timer_lock, timer_running);
            return timeout_status;
        };
    }

    void wait_for(const std::atomic_bool& target_state)
    {
        std::unique_lock<std::mutex> test_lock(cv_m);
        cv.wait(test_lock, [&target_state] { return target_state.load(); });
    }

    std::unique_lock<std::mutex> notify_test_wait()
    {
        std::unique_lock<std::mutex> test_lock(cv_m);
        test_notify_called.store(true);
        cv.notify_all();

        return test_lock;
    }

    void SetUp() override
    {
        ON_CALL(*mock_timer_sync_funcs, wait).WillByDefault(WithArg<1>([this](auto& timer_lock) {
            mock_wait(timer_lock, timer_paused);
        }));

        EXPECT_CALL(*mock_timer_sync_funcs, notify_all)
            .WillOnce(Return())                // Handles the Timer::stop() in Timer::start()
            .WillRepeatedly([this](auto&...) { // Handles all other notify_all calls in Timer
                notify_test_wait();            // discard returned lock
            });
    }

    std::atomic_int timeout_count{0};
    std::atomic_bool test_notify_called{false}, timer_running{false}, timer_paused{false};
    std::condition_variable cv;
    std::mutex cv_m;
    const std::chrono::milliseconds default_timeout{10};
    const std::chrono::seconds default_wait{1};

    MockTimerSyncFuncs::GuardedMock attr{MockTimerSyncFuncs::inject()};
    MockTimerSyncFuncs* mock_timer_sync_funcs = attr.first;
    mpu::Timer t;
};

} // namespace

TEST_F(TestTimer, times_out)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(false)));

    t.start();
    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out yet";

    // Notify the mocked wait_for()
    auto test_lock = notify_test_wait();

    ASSERT_TRUE(cv.wait_for(test_lock, default_wait, [this] { return timeout_count.load() == 1; }))
        << "Did not time out in reasonable timeframe";

    ASSERT_EQ(timeout_count.load(), 1) << "Should have timed out";
}

TEST_F(TestTimer, stops)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)));

    t.start();

    // Wait for the the timer to be "running" in the mocked wait_for()
    wait_for(timer_running);

    // stop() blocks until the Timer thread has joined, ie, it is finished
    t.stop();

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}

TEST_F(TestTimer, pauses)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)));
    EXPECT_CALL(*mock_timer_sync_funcs, wait);

    t.start();

    // Wait for the the timer to be "running" in the mocked wait_for()
    wait_for(timer_running);

    t.pause();

    // Wait for the the timer to be "paused" in the mocked wait()
    wait_for(timer_paused);

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}

TEST_F(TestTimer, resumes)
{
    // The initial start
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)));

    // After resume() is called, there should be less time left than the default timeout
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Lt(default_timeout), _)).WillOnce(Return(false));
    EXPECT_CALL(*mock_timer_sync_funcs, wait);

    t.start();

    // Wait for the the timer to be "running" in the mocked wait_for()
    wait_for(timer_running);

    t.pause();

    // Wait for the the timer to be "paused" in the mocked wait()
    wait_for(timer_paused);

    t.resume();

    std::unique_lock<std::mutex> test_lock(cv_m);
    // Wait on the actual Timer timeout
    ASSERT_TRUE(cv.wait_for(test_lock, default_wait, [this] { return timeout_count.load() == 1; }))
        << "Did not time out in reasonable timeframe";

    ASSERT_EQ(timeout_count.load(), 1) << "Should have timed out";
}

TEST_F(TestTimer, stops_paused)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)));
    EXPECT_CALL(*mock_timer_sync_funcs, wait);

    t.start();

    // Wait for the the timer to be "running" in the mocked wait_for()
    wait_for(timer_running);

    t.pause();

    // Wait for the the timer to be "paused" in the mocked wait()
    wait_for(timer_paused);

    t.stop();

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}

TEST_F(TestTimer, cancels)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)));

    // Don't use the TestTimer mpu::Timer since we are testing it going out of scope
    {
        mpu::Timer scoped_timer{default_timeout, [this] { ++timeout_count; }};
        scoped_timer.start();

        // Wait for the the timer to be "running" in the mocked wait_for()
        wait_for(timer_running);
    }

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}

TEST_F(TestTimer, restarts)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)))
        .WillOnce(WithArg<1>(make_mock_wait_for(false)));

    t.start();

    // Wait for the the timer to be "running" in the mocked wait_for()
    wait_for(timer_running);

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out yet";
    t.start(); // start() will stop the timer if it's running

    // Wait for the the timer to be "running" again in the mocked wait_for()
    wait_for(timer_running);

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out yet";

    // Notify the mocked wait_for()
    auto test_lock = notify_test_wait();

    // Wait on the Timer callback
    ASSERT_TRUE(cv.wait_for(test_lock, default_wait, [this] { return timeout_count.load() > 0; }))
        << "Did not time out in reasonable timeframe";

    ASSERT_EQ(timeout_count.load(), 1) << "Should have timed out once now";
}

TEST_F(TestTimer, stopped_ignores_pause)
{
    // Indicates the Timer was never running
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for).Times(0);

    t.pause();

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}

TEST_F(TestTimer, stopped_ignores_resume)
{
    // Indicates the Timer was never running
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for).Times(0);

    t.resume();

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}

TEST_F(TestTimer, running_ignores_resume)
{
    EXPECT_CALL(*mock_timer_sync_funcs, wait_for(_, _, Eq(default_timeout), _))
        .WillOnce(WithArg<1>(make_mock_wait_for(true)));

    t.start();

    // Wait for the the timer to be "running" in the mocked wait_for()
    wait_for(timer_running);

    t.resume();

    ASSERT_EQ(timeout_count.load(), 0) << "Should not have timed out";
}
