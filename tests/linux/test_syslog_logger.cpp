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

#include <cstdarg>
#include <cstdio>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <src/platform/logger/syslog_logger.h>

namespace mpl = multipass::logging;

class MockSyslog
{
public:
    MOCK_METHOD(int, syslog_send, (int pri, const char* format), ());
};

std::unique_ptr<MockSyslog> mock_syslog{nullptr};

/**
 * weak syslog to intercept syslog calls.
 *
 * This is a cheap way of mocking the thing without having to change anything.
 */
extern "C" __attribute__((weak)) void syslog(int pri, const char* format, ...)
{
    [] { ASSERT_FALSE(nullptr == mock_syslog); }();
    mock_syslog->syslog_send(pri, format);
}

struct syslog_logger_test : ::testing::Test
{

    void SetUp()
    {
        ASSERT_FALSE(nullptr == mock_syslog);
    }

    static void SetUpTestCase()
    {
        mock_syslog = std::make_unique<MockSyslog>();
    }

    static void TearDownTestCase()
    {
        mock_syslog.reset();
    }
};

TEST_F(syslog_logger_test, call_log)
{
    EXPECT_CALL(*mock_syslog, syslog_send(testing::_, testing::_)).Times(1);
    mpl::SyslogLogger uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, "category", "message");
}

TEST_F(syslog_logger_test, call_log_filtered)
{
    EXPECT_CALL(*mock_syslog, syslog_send(testing::_, testing::_)).Times(0);
    mpl::SyslogLogger uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}
