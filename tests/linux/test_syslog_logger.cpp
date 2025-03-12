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

#include <src/platform/logger/syslog_logger.h>

#include <cstdarg>
#include <cstdio>

namespace mpl = multipass::logging;

class MockSyslog
{
public:
    MOCK_METHOD(int, syslog_send, (int pri, const char* format, va_list args), ());
};

std::unique_ptr<MockSyslog> mock_syslog{nullptr};

/**
 * syslog function definition to intercept syslog calls.
 *
 * This is a cheap way of mocking the thing without having to change anything.
 */
extern "C" void syslog(int pri, const char* format, ...)
{
    ASSERT_NE(nullptr, mock_syslog);
    va_list args;
    va_start(args, format);
    mock_syslog->syslog_send(pri, format, args);
    va_end(args);
}

struct syslog_logger_test : ::testing::Test
{
    void SetUp()
    {
        ASSERT_NE(nullptr, mock_syslog);
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
    constexpr static std::string_view expected_category = {"category"};
    constexpr static std::string_view expected_message = {"message"};
    EXPECT_CALL(*mock_syslog, syslog_send(testing::_, testing::_, testing::_))
        .WillOnce([](int pri, const char* format, va_list args) {
            EXPECT_EQ(LOG_DEBUG, pri);
            EXPECT_STREQ(format, "[%.*s] %.*s");

            va_list args_copy;
            va_copy(args_copy, args); // Make a copy to safely extract values

            // Category string view
            const int category_strv_len = va_arg(args_copy, int);
            EXPECT_EQ(category_strv_len, expected_category.size());
            const char* category = va_arg(args_copy, const char*);
            std::string_view category_strv{category, static_cast<std::size_t>(category_strv_len)};
            EXPECT_EQ(category_strv, expected_category);

            // Message string view
            const int message_strv_len = va_arg(args_copy, int);
            EXPECT_EQ(message_strv_len, expected_message.size());
            const char* message = va_arg(args_copy, const char*);
            std::string_view message_strv{message, static_cast<std::size_t>(message_strv_len)};
            EXPECT_EQ(message_strv, expected_message);

            va_end(args_copy); // Cleanup copied va_list
            return true;
        });
    mpl::SyslogLogger uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, expected_category, expected_message);
}

TEST_F(syslog_logger_test, call_log_filtered)
{
    EXPECT_CALL(*mock_syslog, syslog_send(testing::_, testing::_, testing::_)).Times(0);
    mpl::SyslogLogger uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}

struct syslog_logger_priority_test : public testing::TestWithParam<std::tuple<int, mpl::Level>>
{

    void SetUp()
    {
        ASSERT_NE(nullptr, mock_syslog);
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

TEST_P(syslog_logger_priority_test, validate_level_to_priority)
{
    const auto& [syslog_level, mpl_level] = GetParam();

    constexpr static std::string_view expected_category = {"category"};
    constexpr static std::string_view expected_message = {"message"};
    EXPECT_CALL(*mock_syslog, syslog_send(testing::_, testing::_, testing::_))
        .WillOnce([v = syslog_level](int pri, const char* format, va_list args) {
            EXPECT_EQ(v, pri);
            EXPECT_STREQ(format, "[%.*s] %.*s");

            va_list args_copy;
            va_copy(args_copy, args); // Make a copy to safely extract values

            // Category string view
            const int category_strv_len = va_arg(args_copy, int);
            EXPECT_EQ(category_strv_len, expected_category.size());
            const char* category = va_arg(args_copy, const char*);
            std::string_view category_strv{category, static_cast<std::size_t>(category_strv_len)};
            EXPECT_EQ(category_strv, expected_category);

            // Message string view
            const int message_strv_len = va_arg(args_copy, int);
            EXPECT_EQ(message_strv_len, expected_message.size());
            const char* message = va_arg(args_copy, const char*);
            std::string_view message_strv{message, static_cast<std::size_t>(message_strv_len)};
            EXPECT_EQ(message_strv, expected_message);

            va_end(args_copy); // Cleanup copied va_list
            return 0;
        });
    mpl::SyslogLogger uut{mpl_level};
    // This should log
    uut.log(mpl_level, expected_category, expected_message);
}

INSTANTIATE_TEST_SUITE_P(SyslogLevels,
                         syslog_logger_priority_test,
                         ::testing::Values(std::make_tuple(LOG_DEBUG, mpl::Level::trace),
                                           std::make_tuple(LOG_DEBUG, mpl::Level::debug),
                                           std::make_tuple(LOG_ERR, mpl::Level::error),
                                           std::make_tuple(LOG_INFO, mpl::Level::info),
                                           std::make_tuple(LOG_WARNING, mpl::Level::warning)));
