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

#include <multipass/exceptions/formatted_exception_base.h>

#include <gmock/gmock.h> // for EXPECT_THAT
#include <gtest/gtest.h>

struct exception_tests : ::testing::Test
{
};

using testing::HasSubstr;
using testing::ThrowsMessage;

struct custom_exception_type : std::exception
{

    custom_exception_type(const std::string& v) : msg(v)
    {
    }

    const char* what() const noexcept override
    {
        return msg.c_str();
    }

private:
    std::string msg;
};

TEST_F(exception_tests, throw_default)
{
    EXPECT_THAT([]() { throw multipass::FormattedExceptionBase<>("message {}", 1); },
                ThrowsMessage<std::runtime_error>(HasSubstr("message 1")));
}

TEST_F(exception_tests, throw_non_default_std)
{
    EXPECT_THAT([]() { throw multipass::FormattedExceptionBase<std::overflow_error>("message {}", 1); },
                ThrowsMessage<std::overflow_error>(HasSubstr("message 1")));
}

TEST_F(exception_tests, throw_user_defined_exception)
{
    EXPECT_THAT([]() { throw multipass::FormattedExceptionBase<custom_exception_type>("message {}", 1); },
                ThrowsMessage<custom_exception_type>(HasSubstr("message 1")));
}

TEST_F(exception_tests, throw_format_error)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`
Format error: `argument not found`)";
    EXPECT_THAT([]() { throw multipass::FormattedExceptionBase<>("message {}"); },
                ThrowsMessage<std::runtime_error>(HasSubstr(expected_error_msg)));
}
