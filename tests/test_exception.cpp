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
#include <multipass/exceptions/logged_exception_base.h>

#include <tests/common.h>
#include <tests/mock_logger.h>

namespace mpt = multipass::test;

struct AngryTypeThatThrowsUnexpectedThingsOnFormat
{
};

// This is a quick way to make fmt::format throw an "unexpected" thing,
// so we can test the catch-all exception catcher.
template <typename Char>
struct fmt::formatter<AngryTypeThatThrowsUnexpectedThingsOnFormat, Char>
{
    constexpr auto parse(basic_format_parse_context<Char>& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    fmt::context::iterator format(const AngryTypeThatThrowsUnexpectedThingsOnFormat& api, FormatContext& ctx) const
    {
        // What an unusual sight.
        throw int{5};
    }
};
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

struct formatted_exception_base_tests : ::testing::Test
{

    template <typename T>
    struct MockFormattedException : multipass::FormattedExceptionBase<T>
    {
        using multipass::FormattedExceptionBase<T>::FormattedExceptionBase;
    };
};

using testing::HasSubstr;

TEST_F(formatted_exception_base_tests, throw_default)
{
    MP_EXPECT_THROW_THAT(throw multipass::FormattedExceptionBase<>("message {}", 1),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(formatted_exception_base_tests, throw_non_default_std)
{
    MP_EXPECT_THROW_THAT(throw MockFormattedException<std::overflow_error>("message {}", 1),
                         std::overflow_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(formatted_exception_base_tests, throw_std_system_error)
{
    MP_EXPECT_THROW_THAT(
        throw MockFormattedException<std::system_error>(std::make_error_code(std::errc::operation_canceled),
                                                        "message {}",
                                                        1),
        std::system_error,
        mpt::match_what(HasSubstr("message 1")));
}

TEST_F(formatted_exception_base_tests, throw_user_defined_exception)
{
    MP_EXPECT_THROW_THAT(throw MockFormattedException<custom_exception_type>("message {}", 1),
                         custom_exception_type,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(formatted_exception_base_tests, throw_format_error)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`
Format error: `argument not found`)";

    MP_EXPECT_THROW_THAT(throw MockFormattedException<std::runtime_error>("message {}"),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(expected_error_msg)));
}

TEST_F(formatted_exception_base_tests, throw_unexpected_error)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`)";

    MP_EXPECT_THROW_THAT(
        throw MockFormattedException<std::runtime_error>("message {}", AngryTypeThatThrowsUnexpectedThingsOnFormat{}),
        std::runtime_error,
        mpt::match_what(HasSubstr(expected_error_msg)));
}

struct logged_exception_base_tests : ::testing::Test
{
    template <multipass::logging::Level L, typename T>
    struct MockLoggedException : multipass::LoggedExceptionBase<L, T>
    {
        using multipass::LoggedExceptionBase<L, T>::LoggedExceptionBase;
    };

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    using lvl = multipass::logging::Level;
};

TEST_F(logged_exception_base_tests, throw_default)
{
    using uut_t = multipass::LoggedExceptionBase<>;
    logger_scope.mock_logger->expect_log(lvl::error, "message 1");
    // Log error
    MP_EXPECT_THROW_THAT(throw uut_t("category", "message {}", 1),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(logged_exception_base_tests, throw_non_default_std)
{
    using uut_t = multipass::LoggedExceptionBase<lvl::warning, std::overflow_error>;
    logger_scope.mock_logger->expect_log(lvl::warning, "message 1");
    MP_EXPECT_THROW_THAT(throw uut_t("category", "message {}", 1),
                         std::overflow_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(logged_exception_base_tests, throw_std_system_error)
{
    using uut_t = MockLoggedException<lvl::error, std::system_error>;
    logger_scope.mock_logger->expect_log(lvl::error, "message 1");
    MP_EXPECT_THROW_THAT(throw uut_t("category", std::make_error_code(std::errc::operation_canceled), "message {}", 1),
                         std::system_error,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(logged_exception_base_tests, throw_user_defined_exception)
{
    using uut_t = MockLoggedException<lvl::error, custom_exception_type>;
    logger_scope.mock_logger->expect_log(lvl::error, "message 1");

    MP_EXPECT_THROW_THAT(throw uut_t("category", "message {}", 1),
                         custom_exception_type,
                         mpt::match_what(HasSubstr("message 1")));
}

TEST_F(logged_exception_base_tests, throw_format_error)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`
Format error: `argument not found`)";

    using uut_t = MockLoggedException<lvl::error, std::runtime_error>;
    logger_scope.mock_logger->expect_log(lvl::error, expected_error_msg);
    MP_EXPECT_THROW_THAT(throw uut_t("category", "message {}"),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(expected_error_msg)));
}

TEST_F(logged_exception_base_tests, throw_log_error)
{

    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`)";

    ON_CALL(*logger_scope.mock_logger, log(lvl::error, "category", testing::_)).WillByDefault([] {
        throw std::runtime_error{"serious logging issue"};
    });

    logger_scope.mock_logger->expect_log(lvl::error, expected_error_msg);
    using uut_t = MockLoggedException<lvl::error, std::runtime_error>;

    MP_EXPECT_THROW_THAT(throw uut_t("category", "message {}"),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(expected_error_msg)));
}

TEST_F(logged_exception_base_tests, throw_unexpected_error)
{
    constexpr auto expected_error_msg = R"([Error while formatting the exception string]
Format string: `message {}`)";
    using uut_t = MockLoggedException<lvl::error, std::runtime_error>;

    logger_scope.mock_logger->expect_log(lvl::error, expected_error_msg);
    MP_EXPECT_THROW_THAT(throw uut_t("category", "message {}", AngryTypeThatThrowsUnexpectedThingsOnFormat{}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(expected_error_msg)));
}
