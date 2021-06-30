/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#include <multipass/cli/argparser.h>
#include <multipass/cli/command.h>

#include <QString>
#include <QStringList>

#include <sstream>
#include <vector>

namespace mp = multipass;
using namespace testing;

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
