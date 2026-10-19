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

#include <src/platform/logger/syslog_logger.h>
#include <src/platform/logger/syslog_wrapper.h>

#include <cstdarg>
#include <cstdio>

namespace mpl = multipass::logging;

using namespace testing;

struct MockSyslogWrapper : public mpl::SyslogWrapper
{
    using SyslogWrapper::SyslogWrapper;

    MOCK_METHOD(void,
                write_syslog,
                (int, std::string_view, std::string_view, std::string_view),
                (const, override));
    MP_MOCK_SINGLETON_BOILERPLATE(MockSyslogWrapper, mpl::SyslogWrapper);
};

struct SyslogLoggerTest : ::testing::Test
{
    using uut_t = mpl::SyslogLogger;
    MockSyslogWrapper::GuardedMock mock_syslog_guardedmock{MockSyslogWrapper::inject()};
    MockSyslogWrapper& mock_syslog = *mock_syslog_guardedmock.first;
};

TEST_F(SyslogLoggerTest, callLog)
{
    constexpr static std::string_view expected_category = "category";
    constexpr static std::string_view expected_message = "message";
    constexpr static std::string_view expected_fmtstr = "[%.*s] %.*s";
    constexpr static int expected_level = LOG_DEBUG;
    EXPECT_CALL(mock_syslog,
                write_syslog(Eq(expected_level),
                             Eq(expected_fmtstr),
                             Eq(expected_category),
                             Eq(expected_message)));
    uut_t uut{mpl::Level::debug};
    // This should log
    uut.log(mpl::Level::debug, expected_category, expected_message);
}

TEST_F(SyslogLoggerTest, callLogFiltered)
{
    EXPECT_CALL(mock_syslog, write_syslog).Times(0);
    uut_t uut{mpl::Level::debug};
    // This should not log
    uut.log(mpl::Level::trace, "category", "message");
}

struct SyslogLoggerPriorityTest : public testing::TestWithParam<std::tuple<int, mpl::Level>>
{
    using uut_t = mpl::SyslogLogger;
    MockSyslogWrapper::GuardedMock mock_syslog_guardedmock{MockSyslogWrapper::inject()};
    MockSyslogWrapper& mock_syslog = *mock_syslog_guardedmock.first;
};

TEST_P(SyslogLoggerPriorityTest, validateLevelToPriority)
{
    const auto& [syslog_level, mpl_level] = GetParam();
    constexpr static std::string_view expected_category = "category";
    constexpr static std::string_view expected_message = "message";
    constexpr static std::string_view expected_fmtstr = "[%.*s] %.*s";
    EXPECT_CALL(mock_syslog,
                write_syslog(Eq(syslog_level),
                             Eq(expected_fmtstr),
                             Eq(expected_category),
                             Eq(expected_message)));

    uut_t uut{mpl_level};
    // This should log
    uut.log(mpl_level, expected_category, expected_message);
}

INSTANTIATE_TEST_SUITE_P(SyslogLevels,
                         SyslogLoggerPriorityTest,
                         ::testing::Values(std::make_tuple(LOG_DEBUG, mpl::Level::trace),
                                           std::make_tuple(LOG_DEBUG, mpl::Level::debug),
                                           std::make_tuple(LOG_ERR, mpl::Level::error),
                                           std::make_tuple(LOG_INFO, mpl::Level::info),
                                           std::make_tuple(LOG_WARNING, mpl::Level::warning)));
