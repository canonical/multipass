/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include "mock_logger.h"

#include <multipass/logging/log.h>
#include <multipass/top_catch_all.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TopCatchAll : public Test
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs();
    }

    auto make_category_matcher()
    {
        return mpt::MockLogger::make_cstring_matcher(StrEq(category.c_str()));
    }

    const std::string category = "testing";
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

struct CustomExceptionForTesting : public std::exception
{
public:
    CustomExceptionForTesting() = default;
    const char* what() const noexcept override
    {
        return msg;
    }

    inline static constexpr const auto msg = "custom";
};

} // namespace

TEST_F(TopCatchAll, calls_function_with_no_args)
{
    int ret = 123, got = 0;
    EXPECT_NO_THROW(got = mp::top_catch_all("", [ret] { return ret; }););
    EXPECT_EQ(got, ret);
}

TEST_F(TopCatchAll, calls_function_with_args)
{
    int a = 5, b = 7, got = 0;
    EXPECT_NO_THROW(got = mp::top_catch_all("", std::plus<int>{}, a, b););
    EXPECT_EQ(got, a + b);
}

TEST_F(TopCatchAll, handles_unknown_error)
{
    int got = 0;

    EXPECT_CALL(*logger_scope.mock_logger, log(Eq(mpl::Level::error), make_category_matcher(),
                                               mpt::MockLogger::make_cstring_matcher(HasSubstr("unknown"))));
    EXPECT_NO_THROW(got = mp::top_catch_all(category, [] {
                        throw 123;
                        return 0;
                    }););
    EXPECT_EQ(got, EXIT_FAILURE);
}

TEST_F(TopCatchAll, handles_standard_exception)
{
    int got = 0;
    const std::string emsg = "some error";
    const auto msg_matcher = AllOf(HasSubstr("exception"), HasSubstr(emsg.c_str()));

    EXPECT_CALL(*logger_scope.mock_logger, log(Eq(mpl::Level::error), make_category_matcher(),
                                               mpt::MockLogger::make_cstring_matcher(msg_matcher)));
    EXPECT_NO_THROW(got = mp::top_catch_all(category, [&emsg] {
                        throw std::runtime_error{emsg};
                        return 0;
                    }););
    EXPECT_EQ(got, EXIT_FAILURE);
}

TEST_F(TopCatchAll, handles_custom_exception)
{
    int got = 0;
    const auto msg_matcher = AllOf(HasSubstr("exception"), HasSubstr(CustomExceptionForTesting::msg));

    EXPECT_CALL(*logger_scope.mock_logger, log(Eq(mpl::Level::error), make_category_matcher(),
                                               mpt::MockLogger::make_cstring_matcher(msg_matcher)));
    EXPECT_NO_THROW(got = mp::top_catch_all(category, [] {
                        throw CustomExceptionForTesting{};
                        return 42;
                    }));
    EXPECT_EQ(got, EXIT_FAILURE);
}
