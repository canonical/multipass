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

using namespace testing;

struct MockJournaldWrapper : public mpl::JournaldWrapper
{
    using JournaldWrapper::JournaldWrapper;

    MOCK_METHOD(void,
                write_journal,
                (std::string_view,
                 std::string_view,
                 std::string_view,
                 int,
                 std::string_view,
                 std::string_view),
                (const, override));
    MP_MOCK_SINGLETON_BOILERPLATE(MockJournaldWrapper, mpl::JournaldWrapper);
};

struct JournaldLoggerTest : ::testing::Test
{
    using uut_t = mpl::JournaldLogger;
    MockJournaldWrapper::GuardedMock mock_journald_guardedmock{MockJournaldWrapper::inject()};
    MockJournaldWrapper& mock_journald = *mock_journald_guardedmock.first;
};

TEST_F(JournaldLoggerTest, callLog)
{
    constexpr static std::string_view expected_message_fmtstr = "MESSAGE=%.*s";
    constexpr static std::string_view expected_priority_fmtstr = "PRIORITY=%i";
    constexpr static std::string_view expected_category_fmtstr = "CATEGORY=%.*s";

    constexpr static std::string_view expected_category = {"category"};
    constexpr static std::string_view expected_message = {"message"};
    constexpr static auto expected_priority = LOG_DEBUG;

    EXPECT_CALL(mock_journald,
                write_journal(Eq(expected_message_fmtstr),
                              Eq(expected_message),
                              Eq(expected_priority_fmtstr),
                              Eq(expected_priority),
                              Eq(expected_category_fmtstr),
                              Eq(expected_category)));
    uut_t uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, expected_category, expected_message);
}

TEST_F(JournaldLoggerTest, callLogFiltered)
{
    EXPECT_CALL(mock_journald, write_journal).Times(0);
    uut_t uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}
