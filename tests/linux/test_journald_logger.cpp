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

#include "../common.h"
#include "../mock_singleton_helpers.h"

#include <src/platform/logger/journald_logger.h>
#include <src/platform/logger/journald_wrapper.h>

#include <cstdarg>
#include <cstdio>

namespace mpl = multipass::logging;

struct MockJournaldWrapper : public mpl::JournaldWrapper
{
    using JournaldWrapper::JournaldWrapper;

    MOCK_METHOD(void,
                write_journal,
                (std::string_view, std::string_view, std::string_view, int, std::string_view, std::string_view),
                (const, override));
    MP_MOCK_SINGLETON_BOILERPLATE(MockJournaldWrapper, mpl::JournaldWrapper);
};

struct journald_logger_test : ::testing::Test
{
    MockJournaldWrapper::GuardedMock mock_journald_guardedmock{MockJournaldWrapper::inject()};
    MockJournaldWrapper& mock_journald = *mock_journald_guardedmock.first;
};

TEST_F(journald_logger_test, call_log)
{
    constexpr static std::string_view expected_message_fmtstr = "MESSAGE=%.*s";
    constexpr static std::string_view expected_priority_fmtstr = "PRIORITY=%i";
    constexpr static std::string_view expected_category_fmtstr = "CATEGORY=%.*s";

    constexpr static std::string_view expected_category = {"category"};
    constexpr static std::string_view expected_message = {"message"};
    constexpr static auto expected_priority = LOG_DEBUG;

    EXPECT_CALL(mock_journald, write_journal)
        .WillOnce([](std::string_view message_fmtstr,
                     std::string_view message,
                     std::string_view priority_fmtstr,
                     int priority,
                     std::string_view category_fmtstr,
                     std::string_view category) {
            EXPECT_EQ(message_fmtstr, expected_message_fmtstr);
            EXPECT_EQ(message, expected_message);
            EXPECT_EQ(priority_fmtstr, expected_priority_fmtstr);
            EXPECT_EQ(priority, expected_priority);
            EXPECT_EQ(category_fmtstr, expected_category_fmtstr);
            EXPECT_EQ(category, expected_category);
            return 0;
        });
    mpl::JournaldLogger uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, expected_category, expected_message);
}

TEST_F(journald_logger_test, call_log_filtered)
{
    EXPECT_CALL(mock_journald, write_journal).Times(0);
    mpl::JournaldLogger uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}
