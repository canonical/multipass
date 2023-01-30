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
#include "tests/fake_alias_config.h"
#include "tests/stub_terminal.h"

#include <multipass/alias_definition.h>
#include <multipass/cli/argparser.h>
#include <multipass/cli/command.h>

#include <QString>
#include <QStringList>

#include <sstream>
#include <string>
#include <vector>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

struct TestVerbosity : public TestWithParam<int>
{
};

TEST_P(TestVerbosity, test_various_vs)
{
    std::ostringstream oss;
    const auto cmds = std::vector<mp::cmd::Command::UPtr>{};
    const auto v = GetParam();
    auto args = QStringList{"multipass_tests"};

    if (v)
    {
        auto arg = QString{v, 'v'};
        arg.prepend('-');
        args << arg;
    }

    auto parser = mp::ArgParser{args, cmds, oss, oss};
    parser.parse();

    const auto expect = v > 4 ? 4 : v;
    EXPECT_EQ(parser.verbosityLevel(), expect);
}

INSTANTIATE_TEST_SUITE_P(ArgParser, TestVerbosity, Range(0, 10));

struct TestAliasArguments : public TestWithParam<std::tuple<QStringList /* pre */, QStringList /* post */>>,
                            public FakeAliasConfig
{
};

TEST_P(TestAliasArguments, test_alias_arguments)
{
    std::ostringstream oss;
    std::istringstream cin;
    mpt::StubTerminal term(oss, oss, cin);
    const auto cmds = std::vector<mp::cmd::Command::UPtr>{};

    const auto& [pre, post] = GetParam();

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}}});

    mp::AliasDict alias_dict(&term);
    auto parser = mp::ArgParser{pre, cmds, oss, oss};
    const auto& result = parser.parse(alias_dict);

    ASSERT_EQ(result, mp::ParseCode::Ok) << "Failed to parse given arguments";

    EXPECT_EQ(parser.allArguments(), post);
}

INSTANTIATE_TEST_SUITE_P(
    ArgParser, TestAliasArguments,
    Values(std::make_tuple(QStringList{"mp", "an_alias"}, QStringList{"mp", "exec", "an_instance", "a_command"}),
           std::make_tuple(QStringList{"mp", "-v", "an_alias"},
                           QStringList{"mp", "-v", "exec", "an_instance", "a_command"}),
           std::make_tuple(QStringList{"mp", "an_alias", "-v"},
                           QStringList{"mp", "exec", "an_instance", "a_command", "-v"}),
           std::make_tuple(QStringList{"mp", "an_alias", "an_argument"},
                           QStringList{"mp", "exec", "an_instance", "a_command", "an_argument"}),
           std::make_tuple(QStringList{"mp", "an_alias", "--", "an_argument"},
                           QStringList{"mp", "exec", "an_instance", "a_command", "--", "an_argument"}),
           std::make_tuple(QStringList{"mp", "an_alias", "--", "--an_option"},
                           QStringList{"mp", "exec", "an_instance", "a_command", "--", "--an_option"}),
           std::make_tuple(QStringList{"mp", "an_alias", "--", "--an_option", "an_argument"},
                           QStringList{"mp", "exec", "an_instance", "a_command", "--", "--an_option", "an_argument"}),
           std::make_tuple(QStringList{"mp", "an_alias", "an_alias", "an_alias"}, // args happen to be called the same
                           QStringList{"mp", "exec", "an_instance", "a_command", "an_alias", "an_alias"})));
