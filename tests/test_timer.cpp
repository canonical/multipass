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

    std::this_thread::sleep_for(3s); // Windows CI needs longer...
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
