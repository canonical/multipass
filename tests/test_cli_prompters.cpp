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

struct TestPassphrasePrompters : Test
{
    TestPassphrasePrompters()
    {
        EXPECT_CALL(mock_terminal, cout()).WillRepeatedly(ReturnRef(cout));
    }

    std::ostringstream cerr, cout;
    std::istringstream cin;
    mpt::MockTerminal mock_terminal;
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

// Note that the following test does not actually test if the terminal echos or not-
// that is specific to platform terminal types.
TEST_F(TestPassphrasePrompters, passphraseCallsEchoAndReturnsExpectedPassphrase)
{
    const std::string expected_output{"Please enter passphrase: \n"};
    const std::string passphrase{"foo"};

    cin.str(passphrase + "\n");

    {
        InSequence s;

        EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
    }

    EXPECT_CALL(mock_terminal, cin()).WillOnce(ReturnRef(cin));

    mp::PassphrasePrompter prompter{&mock_terminal};

    auto input = prompter.prompt();

    EXPECT_EQ(cout.str(), expected_output);
    EXPECT_EQ(input, passphrase);
}

TEST_F(TestPassphrasePrompters, newPassPhraseCallsEchoAndReturnsExpectedPassphrase)
{
    const std::string expected_output{"Please enter passphrase: \nPlease re-enter passphrase: \n"};
    const std::string passphrase{"foo"};

    {
        InSequence s;

        EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
    }

    EXPECT_CALL(mock_terminal, cin()).Times(2).WillRepeatedly([this, &passphrase]() -> std::istream& {
        cin.str(passphrase + "\n");
        return cin;
    });

    mp::NewPassphrasePrompter prompter{&mock_terminal};

    auto input = prompter.prompt();

    EXPECT_EQ(cout.str(), expected_output);
    EXPECT_EQ(input, passphrase);
}

TEST_F(TestPassphrasePrompters, newPassPhraseWrongPassphraseThrows)
{
    const std::string expected_output{"Please enter passphrase: \nPlease re-enter passphrase: \n"};
    const std::string passphrase{"foo"}, wrong_passphrase{"bar"};

    {
        InSequence s;

        EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
        EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
    }

    auto good_passphrase = [this, &passphrase]() -> std::istream& {
        cin.str(passphrase + "\n");
        return cin;
    };

    auto bad_passphrase = [this, &wrong_passphrase]() -> std::istream& {
        cin.str(wrong_passphrase + "\n");
        return cin;
    };

    EXPECT_CALL(mock_terminal, cin()).WillOnce(Invoke(good_passphrase)).WillOnce(Invoke(bad_passphrase));

    mp::NewPassphrasePrompter prompter{&mock_terminal};

    MP_EXPECT_THROW_THAT(prompter.prompt(), mp::PromptException, mpt::match_what(StrEq("Passphrases do not match")));

    EXPECT_EQ(cout.str(), expected_output);
}

class CLIPromptersBadCinState : public CLIPrompters, public WithParamInterface<std::ios_base::iostate>
{
};

TEST_P(CLIPromptersBadCinState, PlainThrows)
{
    auto prompt = mp::PlainPrompter{&term};

    cin.clear(GetParam());
    MP_EXPECT_THROW_THAT(prompt.prompt(""), mp::PromptException, mpt::match_what(HasSubstr("Failed to read value")));
}

INSTANTIATE_TEST_SUITE_P(CLIPrompters, CLIPromptersBadCinState,
                         Values(std::ios::eofbit, std::ios::failbit, std::ios::badbit));

class BridgePrompterTests : public CLIPrompters,
                            public WithParamInterface<std::tuple<std::vector<std::string>, std::string, bool>>
{
};

TEST_F(CLIPrompters, failsIfNoNetworks)
{
    std::vector<std::string> nets{};

    mpt::MockTerminal mock_terminal;

    mp::BridgePrompter prompter{&mock_terminal};

    EXPECT_CALL(mock_terminal, cin_is_live()).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_terminal, cout_is_live()).WillRepeatedly(Return(false));

    ASSERT_DEBUG_DEATH(prompter.bridge_prompt(nets), "[Aa]ssert");
}

TEST_P(BridgePrompterTests, correctlyReturns)
{
    auto [nets, answer, ret] = GetParam();

    mpt::MockTerminal mock_terminal;
    EXPECT_CALL(mock_terminal, cout()).WillRepeatedly(ReturnRef(cout));
    EXPECT_CALL(mock_terminal, cout_is_live()).WillOnce(Return(true));
    EXPECT_CALL(mock_terminal, cin()).WillOnce(ReturnRef(cin));
    EXPECT_CALL(mock_terminal, cin_is_live()).WillOnce(Return(true));

    cin.str(answer + "\n");

    mp::BridgePrompter prompter{&mock_terminal};

    EXPECT_EQ(prompter.bridge_prompt(nets), ret);
}

INSTANTIATE_TEST_SUITE_P(CLIPrompters,
                         BridgePrompterTests,
                         Values(std::make_tuple(std::vector<std::string>{"eth1"}, "yes", true),
                                std::make_tuple(std::vector<std::string>{"eth1", "eth3"}, "y", true),
                                std::make_tuple(std::vector<std::string>{"eth1", "eth3"}, "no", false),
                                std::make_tuple(std::vector<std::string>{"eth1"}, "n", false)));

TEST_F(CLIPrompters, handlesWrongAnswer)
{
    mpt::MockTerminal mock_terminal;
    EXPECT_CALL(mock_terminal, cout()).WillRepeatedly(ReturnRef(cout));
    EXPECT_CALL(mock_terminal, cout_is_live()).WillOnce(Return(true));
    EXPECT_CALL(mock_terminal, cin()).WillRepeatedly(ReturnRef(cin));
    EXPECT_CALL(mock_terminal, cin_is_live()).WillOnce(Return(true));

    cin.str("qqq\nyes\n");

    mp::BridgePrompter prompter{&mock_terminal};

    std::vector<std::string> nets{"eth2"};

    EXPECT_EQ(prompter.bridge_prompt(nets), true);
}

TEST_F(CLIPrompters, falseOnNonLiveTerminal)
{
    mpt::MockTerminal mock_terminal;
    EXPECT_CALL(mock_terminal, cin_is_live()).WillOnce(Return(false));

    mp::BridgePrompter prompter{&mock_terminal};

    std::vector<std::string> nets{"eth2"};

    EXPECT_EQ(prompter.bridge_prompt(nets), false);
}
