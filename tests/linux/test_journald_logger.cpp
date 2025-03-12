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

#include <src/platform/logger/journald_logger.h>

#include <cstdarg>
#include <cstdio>

namespace mpl = multipass::logging;

class MockSyslog
{
public:
    MOCK_METHOD(int, journal_send, (const char* format, va_list args), ());
};

std::unique_ptr<MockSyslog> mock_journal{nullptr};

/**
 * weak sd_journal_send to intercept journal calls.
 *
 * This is a cheap way of mocking the thing without having to change anything.
 */
extern "C" int sd_journal_send(const char* format, ...)
{
    [] { ASSERT_NE(nullptr, mock_journal); }();

    va_list args;
    va_start(args, format);
    mock_journal->journal_send(format, args);
    va_end(args);
    return 0;
}

struct journald_logger_test : ::testing::Test
{

    void SetUp()
    {
        ASSERT_NE(nullptr, mock_journal);
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
    constexpr static std::string_view expected_category = {"category"};
    constexpr static std::string_view expected_message = {"message"};

    EXPECT_CALL(*mock_journal, journal_send(testing::_, testing::_)).WillOnce([](const char* format, va_list args) {
        EXPECT_STREQ(format, "MESSAGE=%.*s");

        // Extract the va_list
        // We have to be careful here and respect the original variable types.
        va_list args_copy;
        va_copy(args_copy, args); // Make a copy to safely extract values

        // Message string view
        const int message_strv_len = va_arg(args_copy, int);
        EXPECT_EQ(message_strv_len, expected_message.size());
        const char* message = va_arg(args_copy, const char*);
        std::string_view message_strv{message, static_cast<std::size_t>(message_strv_len)};
        EXPECT_EQ(message_strv, expected_message);

        // Priority format string (this one's NUL terminated)
        const char* priority_fmtstr = va_arg(args_copy, const char*);
        EXPECT_STREQ("PRIORITY=%i", priority_fmtstr);
        const int priority = va_arg(args_copy, int);
        EXPECT_EQ(priority, LOG_DEBUG);

        // Category format string (this one's NUL terminated)
        const char* category_fmtstr = va_arg(args_copy, const char*);
        EXPECT_STREQ("CATEGORY=%.*s", category_fmtstr);

        // Category string view
        const int category_strv_len = va_arg(args_copy, int);
        EXPECT_EQ(category_strv_len, expected_category.size());
        const char* category = va_arg(args_copy, const char*);
        std::string_view category_strv{category, static_cast<std::size_t>(category_strv_len)};
        EXPECT_EQ(category_strv, expected_category);

        // Ending null
        const void* terminating_nullptr = va_arg(args_copy, const void*);
        EXPECT_EQ(terminating_nullptr, nullptr);

        va_end(args_copy); // Cleanup copied va_list
        return 0;
    });
    mpl::JournaldLogger uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, expected_category, expected_message);
}

TEST_F(journald_logger_test, call_log_filtered)
{
    EXPECT_CALL(*mock_journal, journal_send(testing::_, testing::_)).Times(0);
    mpl::JournaldLogger uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}
