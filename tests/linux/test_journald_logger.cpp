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
#include <src/platform/logger/journald_logger.h>

namespace mpl = multipass::logging;

class MockSyslog
{
public:
    MOCK_METHOD(int, journal_send, (const char* format), ());
};

std::unique_ptr<MockSyslog> mock_journal{nullptr};

/**
 * weak sd_journal_send to intercept journal calls.
 *
 * This is a cheap way of mocking the thing without having to change anything.
 */
extern "C" __attribute__((weak)) int sd_journal_send(const char* format, ...)
{
    [] { ASSERT_FALSE(nullptr == mock_journal); }();
    mock_journal->journal_send(format);
    return 0;
}

struct journald_logger_test : ::testing::Test
{

    void SetUp()
    {
        ASSERT_FALSE(nullptr == mock_journal);
    }

    static void SetUpTestCase()
    {
        mock_journal = std::make_unique<MockSyslog>();
    }

    static void TearDownTestCase()
    {
        mock_journal.reset();
    }
};

TEST_F(journald_logger_test, call_log)
{
    EXPECT_CALL(*mock_journal, journal_send(testing::_)).Times(1).WillOnce(testing::Return(0));
    mpl::JournaldLogger uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, "category", "message");
}

TEST_F(journald_logger_test, call_log_filtered)
{
    EXPECT_CALL(*mock_journal, journal_send(testing::_)).Times(0);
    mpl::JournaldLogger uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}
