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

#include <multipass/timer.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace mp = multipass;
namespace mpu = multipass::utils;

using namespace std::chrono_literals;
using namespace testing;

namespace
{
struct Timer : public testing::Test
{
    std::atomic_bool timedout = false;
};
} // namespace

TEST_F(Timer, times_out)
{
    mpu::Timer t{1s, [this]() { timedout.store(true); }};
    t.start();
    ASSERT_FALSE(timedout.load()) << "Should not have timed out yet";

    std::this_thread::sleep_for(5s); // Windows CI needs longer...
    ASSERT_TRUE(timedout.load()) << "Should have timed out";
}

TEST_F(Timer, stops)
{
    mpu::Timer t{1s, [this]() { timedout.store(true); }};
    t.start();
    ASSERT_FALSE(timedout.load()) << "Should not have timed out yet";

    t.stop();

    std::this_thread::sleep_for(2s);
    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, pauses)
{
    mpu::Timer t{1s, [this]() { timedout.store(true); }};
    t.start();
    ASSERT_FALSE(timedout.load()) << "Should not have timed out yet";

    t.pause();

    std::this_thread::sleep_for(2s);
    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, resumes)
{
    mpu::Timer t{1s, [this]() { timedout.store(true); }};
    t.start();
    ASSERT_FALSE(timedout.load()) << "Should not have timed out yet";

    t.pause();

    std::this_thread::sleep_for(2s);
    ASSERT_FALSE(timedout.load()) << "Should not have timed out yet";

    t.resume();

    std::this_thread::sleep_for(2s);
    ASSERT_TRUE(timedout.load()) << "Should have timed out now";
}

TEST_F(Timer, stops_paused)
{
    mpu::Timer t{1s, [this]() { timedout.store(true); }};
    t.start();
    ASSERT_FALSE(timedout.load()) << "Should not have timed out yet";

    t.pause();
    t.stop();

    std::this_thread::sleep_for(2s);
    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, cancels)
{
    {
        mpu::Timer t{1s, [this]() { timedout.store(true); }};
        t.start();
    }
    ASSERT_FALSE(timedout.load()) << "Should not have timed out";

    std::this_thread::sleep_for(2s);
    ASSERT_FALSE(timedout.load()) << "Should not have timed out still";
}

TEST_F(Timer, restarts)
{
    int count = 0;
    std::mutex count_m;

    mpu::Timer t{2s, [&count, &count_m]() {
                     std::lock_guard<std::mutex> lk{count_m};
                     ++count;
                 }};

    t.start();
    std::this_thread::sleep_for(1s);

    t.start();
    std::this_thread::sleep_for(1s);
    {
        std::lock_guard<std::mutex> lk{count_m};
        ASSERT_EQ(count, 0) << "Should not have timed out yet";
    }

    std::this_thread::sleep_for(5s); // Windows CI needs longer...
    {
        std::lock_guard<std::mutex> lk{count_m};
        ASSERT_EQ(count, 1) << "Should have timed out once now";
    }
}

TEST_F(Timer, stopped_ignores_pause)
{
    mpu::Timer t{0s, [this]() { timedout.store(true); }};

    t.pause();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, stopped_ignores_resume)
{
    mpu::Timer t{0s, [this]() { timedout.store(true); }};

    t.resume();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}

TEST_F(Timer, running_ignores_resume)
{
    mpu::Timer t{0s, [this]() { timedout.store(true); }};

    t.start();
    t.resume();

    ASSERT_FALSE(timedout.load()) << "Should not have timed out";
}
