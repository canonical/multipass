/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_EXTRA_ASSERTIONS_H
#define MULTIPASS_EXTRA_ASSERTIONS_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QString>

// Extra macros for testing exceptions.
//
//    * MP_{ASSERT|EXPECT}_THROW_THAT(statement, expected_exception, matcher):
//         Tests that the statement throws an exception of the expected type, matching the provided matcher
#define MP_EXPECT_THROW_THAT(statement, expected_exception, matcher)                                                   \
    EXPECT_THROW(                                                                                                      \
        {                                                                                                              \
            try                                                                                                        \
            {                                                                                                          \
                statement;                                                                                             \
            }                                                                                                          \
            catch (const expected_exception& e)                                                                        \
            {                                                                                                          \
                EXPECT_THAT(e, matcher);                                                                               \
                throw;                                                                                                 \
            }                                                                                                          \
        },                                                                                                             \
        expected_exception)

#define MP_ASSERT_THROW_THAT(statement, expected_exception, matcher)                                                   \
    ASSERT_THROW(                                                                                                      \
        {                                                                                                              \
            try                                                                                                        \
            {                                                                                                          \
                statement;                                                                                             \
            }                                                                                                          \
            catch (const expected_exception& e)                                                                        \
            {                                                                                                          \
                ASSERT_THAT(e, matcher);                                                                               \
                throw;                                                                                                 \
            }                                                                                                          \
        },                                                                                                             \
        expected_exception)

namespace multipass::test
{
template <typename MsgMatcher>
auto match_what(MsgMatcher&& matcher)
{
    return testing::Property(&std::exception::what, std::forward<MsgMatcher>(matcher));
}

template <typename StrMatcher>
auto match_qstring(StrMatcher&& matcher)
{
    return testing::Property(&QString::toStdString, std::forward<StrMatcher>(matcher));
}
} // namespace multipass::test

#endif // MULTIPASS_EXTRA_ASSERTIONS_H
