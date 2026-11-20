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

#include "common.h"
#include "mock_terminal.h"
#include "stub_terminal.h"

#include <sstream>
#include <string>

namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TestTerminal : public Test
{
    std::stringstream cin_stream, cout_stream, cerr_stream;
};

using LivenessParam = std::tuple<bool, bool, bool>; // cin live, cout live, expected is_live
struct TestTerminalLiveness : public TestWithParam<LivenessParam>
{
};
} // namespace

TEST_P(TestTerminalLiveness, isLiveRequiresBothCinAndCoutLive)
{
    const auto [cin_live, cout_live, expected] = GetParam();
    NiceMock<mpt::MockTerminal> term;
    ON_CALL(term, cin_is_live).WillByDefault(Return(cin_live));
    ON_CALL(term, cout_is_live).WillByDefault(Return(cout_live));

    EXPECT_EQ(term.is_live(), expected);
}

INSTANTIATE_TEST_SUITE_P(Terminal,
                         TestTerminalLiveness,
                         Values(LivenessParam{true, true, true},
                                LivenessParam{true, false, false},
                                LivenessParam{false, true, false},
                                LivenessParam{false, false, false}));

TEST_F(TestTerminal, readAllCinReturnsEmptyStringOnEmptyInput)
{
    mpt::StubTerminal term{cout_stream, cerr_stream, cin_stream};

    EXPECT_THAT(term.read_all_cin(), IsEmpty());
}

TEST_F(TestTerminal, readAllCinReturnsWholeInput)
{
    const std::string input{"first line\nsecond line\n"};
    cin_stream << input;
    mpt::StubTerminal term{cout_stream, cerr_stream, cin_stream};

    EXPECT_EQ(term.read_all_cin(), input);
}

TEST_F(TestTerminal, readAllCinHandlesInputLargerThanInternalBuffer)
{
    const std::string input(5000, 'x');
    cin_stream << input;
    mpt::StubTerminal term{cout_stream, cerr_stream, cin_stream};

    EXPECT_EQ(term.read_all_cin(), input);
}
