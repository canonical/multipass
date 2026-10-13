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

namespace multipass::test
{

using namespace ::testing;

namespace
{

template <typename... Args>
auto make_arguments_provider(Args&&... args)
{
    return [args = std::vector<std::string>{std::forward<Args>(args)...}]() { return args; };
}

template <typename... Args>
std::vector<AutoCompleteParameterSpecs> single_parameter(Args&&... args)
{
    return {{make_arguments_provider(std::forward<Args>(args)...)}};
}

} // namespace

TEST(AutoCompleter, autoCompletesPositionalParameters)
{
    auto specs = AutoCompleteCommandSpecs{
        .positional_parameters = {{make_arguments_provider("carotte", "concombre", "courgette")},
                                  {make_arguments_provider("patate", "radis")}},
    };

    {
        auto completer = AutoCompleter{specs};
        EXPECT_THAT(completer.complete({}), ElementsAre("carotte", "concombre", "courgette"));
        EXPECT_THAT(completer.complete({"first"}), ElementsAre("patate", "radis"));
        EXPECT_THAT(completer.complete({"first", "second"}), ElementsAre());
    }

    {
        specs.last_repeatable = true;

        auto completer = AutoCompleter{specs};
        EXPECT_THAT(completer.complete({"first", "second"}), ElementsAre("patate", "radis"));
        EXPECT_THAT(completer.complete({"first", "patate"}), ElementsAre("radis"));
        EXPECT_THAT(completer.complete({"first", "radis", "patate"}), ElementsAre());
    }
}

TEST(AutoCompleter, autoCompletesOptions)
{
    auto completer = AutoCompleter{AutoCompleteCommandSpecs{
        .optional_parameters = {{"--opt1", {}}, {"--opt2", {}}, {"--option3", {}}}}};

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2", "--option3"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("--opt2", "--option3"));
    EXPECT_THAT(completer.complete({"--opt1", "--option3"}), ElementsAre("--opt2"));
}

TEST(AutoCompleter, autoCompletesOptionParameters)
{
    auto completer = AutoCompleter{AutoCompleteCommandSpecs{
        .optional_parameters =
            {{"--opt1", AutoCompleteOptionSpecs{.parameters = single_parameter("a1", "a2")}},
             {"--opt2", AutoCompleteOptionSpecs{.parameters = single_parameter("b1", "b2", "b3")}}},
        .option_aliases = {{"-a", "--opt1"}}}};

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("a1", "a2"));
    EXPECT_THAT(completer.complete({"--opt1", "a2"}), ElementsAre("--opt2"));
    EXPECT_THAT(completer.complete({"--opt1", "a2", "a1"}), ElementsAre("--opt2"));
    EXPECT_THAT(completer.complete({"--opt1", "a2", "--opt2"}), ElementsAre("b1", "b2", "b3"));

    EXPECT_THAT(completer.complete({"-a"}), ElementsAre("a1", "a2"));
    EXPECT_THAT(completer.complete({"-a", "a1"}), ElementsAre("--opt2"));
    EXPECT_THAT(completer.complete({"-a", "a1", "--opt2"}), ElementsAre("b1", "b2", "b3"));
}

TEST(AutoCompleter, autoCompletesRepeatableOptions)
{
    auto completer = AutoCompleter{AutoCompleteCommandSpecs{
        .optional_parameters = {{"--opt1", AutoCompleteOptionSpecs{.repeatable = true}},
                                {"--opt2",
                                 AutoCompleteOptionSpecs{
                                     .parameters = single_parameter("v1", "v2"),
                                     .repeatable = true,
                                 }}}}};

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt1", "--opt1"}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt2"}), ElementsAre("v1", "v2"));
    EXPECT_THAT(completer.complete({"--opt2", "v1"}), ElementsAre("--opt1", "--opt2"));
    EXPECT_THAT(completer.complete({"--opt2", "v1", "--opt2"}), ElementsAre("v1", "v2"));
}

TEST(AutoCompleter, canSetMutuallyExclusiveOptions)
{
    auto specs = {
        AutoCompleteCommandSpecs{
            .optional_parameters = {{"--opt1",
                                     AutoCompleteOptionSpecs{.excluded_options = {"--opt2"}}},
                                    {"--opt2", {}},
                                    {"--opt3", {}}},
            .option_aliases = {{"-a", "--opt1"}}},
        AutoCompleteCommandSpecs{
            .optional_parameters = {{"--opt1", {}},
                                    {"--opt2",
                                     AutoCompleteOptionSpecs{.excluded_options = {"--opt1"}}},
                                    {"--opt3", {}}},
            .option_aliases = {{"-a", "--opt1"}}},
        AutoCompleteCommandSpecs{
            .optional_parameters = {{"--opt1",
                                     AutoCompleteOptionSpecs{.excluded_options = {"--opt2"}}},
                                    {"--opt2",
                                     AutoCompleteOptionSpecs{.excluded_options = {"--opt1"}}},
                                    {"--opt3", {}}},
            .option_aliases = {{"-a", "--opt1"}}},
    };

    for (const auto& spec : specs)
    {
        auto completer = AutoCompleter{spec};
        EXPECT_THAT(completer.complete({}), ElementsAre("--opt1", "--opt2", "--opt3"));
        EXPECT_THAT(completer.complete({"--opt1"}), ElementsAre("--opt3"));
        EXPECT_THAT(completer.complete({"--opt2"}), ElementsAre("--opt3"));
        EXPECT_THAT(completer.complete({"-a"}), ElementsAre("--opt3"));
    }
}

TEST(AutoCompleter, canSetMutuallyExclusiveOptionAndParameter)
{
    auto completer = AutoCompleter{AutoCompleteCommandSpecs{
        .optional_parameters = {{"--opt", AutoCompleteOptionSpecs{.excluded_parameters = {0}}}},
        .option_aliases = {{"-a", "--opt"}},
        .positional_parameters = single_parameter("v1", "v2"),
    }};

    EXPECT_THAT(completer.complete({}), ElementsAre("--opt", "v1", "v2"));
    EXPECT_THAT(completer.complete({"--opt"}), ElementsAre());
    EXPECT_THAT(completer.complete({"-a"}), ElementsAre());
    EXPECT_THAT(completer.complete({"v1"}), ElementsAre());
    EXPECT_THAT(completer.complete({"v2"}), ElementsAre());
}

TEST(AutoCompleter, complexAutoComplete)
{
    auto completer = AutoCompleter{AutoCompleteCommandSpecs{
        .optional_parameters =
            {
                {"--format",
                 AutoCompleteOptionSpecs{.parameters = single_parameter("csv", "json", "table")}},
                {"--remotes",
                 AutoCompleteOptionSpecs{.excluded_options = {"--all"},
                                         .excluded_parameters = {0, 1}}},
                {"--all", {}},
                {"--help", {}},
                {"--verbose", AutoCompleteOptionSpecs{.repeatable = true}},
            },
        .option_aliases = {{"-f", "--format"}, {"-a", "--all"}, {"-h", "--help"}},
        .positional_parameters =
            {
                {make_arguments_provider("vma", "vmb")},
                {make_arguments_provider("vm1", "vm2", "vm3")},
            },
        .last_repeatable = true}};

    EXPECT_THAT(completer.complete({}),
                ElementsAre("--all", "--format", "--help", "--remotes", "--verbose", "vma", "vmb"));

    EXPECT_THAT(completer.complete({"vma", "--format"}), ElementsAre("csv", "json", "table"));
    EXPECT_THAT(completer.complete({"vma", "-f"}), ElementsAre("csv", "json", "table"));

    EXPECT_THAT(completer.complete({"--all"}),
                ElementsAre("--format", "--help", "--verbose", "vma", "vmb"));
    EXPECT_THAT(completer.complete({"-a"}),
                ElementsAre("--format", "--help", "--verbose", "vma", "vmb"));

    EXPECT_THAT(completer.complete({"vma", "--all"}),
                ElementsAre("--format", "--help", "--verbose", "vm1", "vm2", "vm3"));

    EXPECT_THAT(completer.complete({"--remotes", "--verbose"}),
                ElementsAre("--format", "--help", "--verbose"));

    EXPECT_THAT(completer.complete({"--help", "vma", "vm1"}),
                ElementsAre("--all", "--format", "--verbose", "vm2", "vm3"));
}

} // namespace multipass::test
