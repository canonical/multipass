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

#include <multipass/cli/autocompleter.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <list>
#include <ranges>
#include <set>
#include <string_view>

namespace multipass::test
{

using namespace ::testing;

TEST(AutoCompleter, autoCompletesPositionalParameters)
{
    auto completer = AutoCompleter{};

    completer.add_parameter({"carotte", "concombre", "courgette"});
    completer.add_parameter({"patate", "radis"});

    EXPECT_THAT(completer.complete({}), ElementsAre("carotte", "concombre", "courgette"));
    EXPECT_THAT(completer.complete({"first"}), ElementsAre("patate", "radis"));
    EXPECT_THAT(completer.complete({"first", "second"}), ElementsAre());

    completer.set_repeat_last_parameter(true);
    EXPECT_THAT(completer.complete({"first", "second"}), ElementsAre("patate", "radis"));
    EXPECT_THAT(completer.complete({"first", "patate"}), ElementsAre("radis"));
    EXPECT_THAT(completer.complete({"first", "radis", "patate"}), ElementsAre());
}

TEST(AutoCompleter, autoCompletesOptions)
{
    auto completer = AutoCompleter{};
    completer.add_option("opt1");
    completer.add_option("opt2");
    completer.add_option("option3");

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2", "--option3"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("--opt2", "--option3"));
    EXPECT_THAT(completer.complete({"--opt1", "--option3"}), ElementsAre("--opt2"));
}

TEST(AutoCompleter, autoCompletesOptionParameters)
{
    auto completer = AutoCompleter{};

    const auto opt1 = completer.add_option("opt1");
    completer.add_option_parameter(opt1, []() { return std::vector<std::string>{"a1", "a2"}; });

    const auto opt2 = completer.add_option("opt2");
    completer.add_option_parameter(opt2,
                                   []() { return std::vector<std::string>{"b1", "b2", "b3"}; });

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("a1", "a2"));
    EXPECT_THAT(completer.complete({"--opt1", "a2"}), ElementsAre("--opt2"));
    EXPECT_THAT(completer.complete({"--opt1", "a2", "a1"}), ElementsAre("--opt2"));
    EXPECT_THAT(completer.complete({"--opt1", "a2", "--opt2"}), ElementsAre("b1", "b2", "b3"));
}

TEST(AutoCompleter, autoCompletesRepeatableOptions)
{
    auto completer = AutoCompleter{};

    completer.add_option("opt1", true);

    const auto opt2 = completer.add_option("opt2", true);
    completer.add_option_parameter(opt2, []() { return std::vector<std::string>{"v1", "v2"}; });

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt1", "--opt1"}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt2"}), ElementsAre("v1", "v2"));
    EXPECT_THAT(completer.complete({"--opt2", "v1"}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt2", "v1", "--opt2"}), ElementsAre("v1", "v2"));
}

TEST(AutoCompleter, canSetMutuallyExclusiveOptions)
{
    auto completer = AutoCompleter{};

    const auto opt1 = completer.add_option("opt1");
    const auto opt2 = completer.add_option("opt2");
    completer.add_option("opt3");

    completer.set_mutual_exclusion(opt1, opt2);

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2", "--opt3"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("--opt3"));
    EXPECT_THAT(completer.complete({"--opt2"}), ElementsAre("--opt3"));
}

TEST(AutoCompleter, canSetMutuallyExclusiveOptionAndParameter)
{
    auto completer = AutoCompleter{};

    const auto opt = completer.add_option("opt");
    const auto param = completer.add_parameter(
        []() { return std::vector<std::string>{"v1", "v2"}; });

    completer.set_mutual_exclusion(opt, param);

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt", "v1", "v2"));
    EXPECT_THAT(completer.complete({"--opt"}), ElementsAre());
    EXPECT_THAT(completer.complete({"v1"}), ElementsAre());
    EXPECT_THAT(completer.complete({"v2"}), ElementsAre());
}

TEST(AutoCompleter, complexAutoComplete)
{
    auto completer = AutoCompleter{};

    const auto format = completer.add_option("format");
    completer.add_option_parameter(format, []() {
        return std::vector<std::string>{"csv", "json", "table"};
    });

    const auto remotes = completer.add_option("remotes");
    const auto all = completer.add_option("all");

    completer.add_option("help");
    completer.add_option("verbose", true);

    const auto param = completer.add_parameter(
        []() { return std::vector<std::string>{"vm1", "vm2", "vm3"}; });

    completer.set_mutual_exclusion(remotes, all);
    completer.set_mutual_exclusion(remotes, param);
    completer.set_repeat_last_parameter(true);

    EXPECT_THAT(
        completer.complete({}),
        ElementsAre("--all", "--format", "--help", "--remotes", "--verbose", "vm1", "vm2", "vm3"));

    EXPECT_THAT(completer.complete({"vm1", "--format"}), ElementsAre("csv", "json", "table"));

    EXPECT_THAT(completer.complete({"--all"}),
                ElementsAre("--format", "--help", "--verbose", "vm1", "vm2", "vm3"));

    EXPECT_THAT(completer.complete({"--remotes", "--verbose"}),
                ElementsAre("--format", "--help", "--verbose"));

    EXPECT_THAT(completer.complete({"--help", "vm1"}),
                ElementsAre("--all", "--format", "--verbose", "vm2", "vm3"));
}

} // namespace multipass::test
