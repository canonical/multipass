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

#ifndef MULTIPASS_COMMON_H
#define MULTIPASS_COMMON_H

#include <multipass/format.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>

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

// work around warning: https://github.com/google/googletest/issues/2271#issuecomment-665742471
#define MP_TYPED_TEST_SUITE(suite_name, types_param) TYPED_TEST_SUITE(suite_name, types_param, )

// Teach GTest to print Qt stuff
QT_BEGIN_NAMESPACE
class QString;
void PrintTo(const QString& qstr, std::ostream* os);
QT_END_NAMESPACE

// Teach GTest to print multipass stuff
namespace multipass
{
struct NetworkInterface;
struct NetworkInterfaceInfo;

void PrintTo(const NetworkInterface& net, std::ostream* os);
void PrintTo(const NetworkInterfaceInfo& net, std::ostream* os);
} // namespace multipass

// Matchers
namespace multipass::test
{

/**
 * Adapt an n-ary callable to a unary callable receiving an n-tuple.
 * @details This is useful to create matchers with Truly (which expects a unary predicate).
 * @tparam F The type of the callable.
 * @param f The n-ary callable we want to adapt. Note that this argument may be copied.
 * @return A unary callable that receives an n-tuple, calls \c f with that tuple unpacked, and returns what @c f returns
 */
template <typename F>
auto with_arg_tuple(F f)
{
    return [f](auto&& arg_tuple) // may copy f, but avoiding forwarding-capture mess (see https://v.gd/2IbEdv)
    { return std::apply(f, std::forward<decltype(arg_tuple)>(arg_tuple)); };
}

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

#endif // MULTIPASS_COMMON_H
