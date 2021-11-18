/*
 * Copyright (C) 2021 Canonical, Ltd.
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
#include "stub_terminal.h"

#include <multipass/cli/prompters.h>
#include <multipass/exceptions/cli_exceptions.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct CLIPrompters : Test
{
    std::ostringstream cerr, cout;
    std::istringstream cin;
    mpt::StubTerminal term{cout, cerr, cin};
};

TEST_F(CLIPrompters, PlainPromptsText)
{
    auto prompt = mp::PlainPrompter{&term};
    cin.str("\n");
    prompt.prompt("foo");

    EXPECT_EQ(cout.str(), "foo: ");
}

TEST_F(CLIPrompters, PlainReturnsText)
{
    auto prompt = mp::PlainPrompter{&term};
    cin.str("value\n");

    EXPECT_EQ(prompt.prompt(""), "value");
}

class CLIPromptersCinState : public CLIPrompters, public WithParamInterface<std::ios_base::iostate>
{
};

TEST_P(CLIPromptersCinState, PlainThrows)
{
    auto prompt = mp::PlainPrompter{&term};

    cin.clear(GetParam());
    MP_EXPECT_THROW_THAT(prompt.prompt(""), mp::PromptException, mpt::match_what(HasSubstr("Failed to read value")));
}

INSTANTIATE_TEST_SUITE_P(BadCin, CLIPromptersCinState, Values(std::ios::eofbit, std::ios::failbit, std::ios::badbit));
