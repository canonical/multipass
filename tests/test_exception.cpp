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
#include <tests/common.h>

namespace mpt = multipass::test;

struct ExceptionTests : ::testing::Test
{
};

using testing::HasSubstr;

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

template <typename T>
struct MockException : multipass::FormattedExceptionBase<T>
{
    using multipass::FormattedExceptionBase<T>::FormattedExceptionBase;
};

struct AngryTypeThatThrowsUnexpectedThings
{
};

// This is a quick way to make fmt::format throw an "unexpected" thing,
// so we can test the catch-all exception catcher.
template <typename Char>
struct fmt::formatter<AngryTypeThatThrowsUnexpectedThings, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    fmt::context::iterator format(const AngryTypeThatThrowsUnexpectedThings& api,
                                  FormatContext& ctx) const
    {
        // What an unusual sight.
        throw int{5};
    }
};

TEST_F(ExceptionTests, throwDefault)
{
    MP_EXPECT_THROW_THAT(throw multipass::FormattedExceptionBase<>("message {}", 1),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(ExceptionTests, throwNonDefaultStd)
{
    MP_EXPECT_THROW_THAT(throw MockException<std::overflow_error>("message {}", 1),
                         std::overflow_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(ExceptionTests, throwStdSystemError)
{
    MP_EXPECT_THROW_THAT(
        throw MockException<std::system_error>(std::make_error_code(std::errc::operation_canceled),
                                               "message {}",
                                               1),
        std::system_error,
        mpt::match_what(HasSubstr("message 1")));
}

TEST_F(ExceptionTests, throwUserDefinedException)
{
    MP_EXPECT_THROW_THAT(throw MockException<custom_exception_type>("message {}", 1),
                         custom_exception_type,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(ExceptionTests, throwFormatError)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`
Format error: `argument not found`)";

    MP_EXPECT_THROW_THAT(throw MockException<std::runtime_error>(fmt::runtime("message {}")),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(expected_error_msg)));
}

TEST_F(ExceptionTests, throwUnexpectedError)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`)";

    MP_EXPECT_THROW_THAT(
        throw MockException<std::runtime_error>("message {}",
                                                AngryTypeThatThrowsUnexpectedThings{}),
        std::runtime_error,
        mpt::match_what(HasSubstr(expected_error_msg)));
}
