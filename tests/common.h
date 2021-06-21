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
#include <multipass/network_interface.h>
#include <multipass/network_interface_info.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QString>

#include <algorithm>
#include <ostream>

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

namespace multipass
{
inline void PrintTo(const NetworkInterface& net, std::ostream* os) // teaching gtest to print NetworkInterface
{
    *os << "NetworkInterface(id=\"" << net.id << "\")";
}

inline void PrintTo(const NetworkInterfaceInfo& net, std::ostream* os) // teaching gtest to print NetworkInterfaceInfo
{
    *os << "NetworkInterfaceInfo(id=\"" << net.id << "\")";
}
} // namespace multipass

namespace multipass::test
{
MATCHER_P(ContainedIn, container, "")
{
    return std::find(std::cbegin(container), std::cend(container), arg) != std::cend(container);
}

MATCHER_P2(HasCorrespondentIn, container, binary_pred,
           fmt::format("{} a corresponding element in {} that pairs with it according to the given binary predicate",
                       negation ? "has" : "doesn't have", testing::PrintToString(container)))
{
    return std::any_of(std::cbegin(container), std::cend(container),
                       [this, &arg](const auto& elem) { return binary_pred(arg, elem); });
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
