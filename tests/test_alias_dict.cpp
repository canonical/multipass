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

#include <multipass/cli/alias_dict.h>

#include <gmock/gmock.h>

#include "fake_alias_config.h"
#include "file_operations.h"
#include "mock_standard_paths.h"

#include "src/client/common/client_formatter.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AliasDictionary : public FakeAliasConfig, public Test
{
    AliasDictionary()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(fake_alias_dir.path()));
    }
};

TEST_F(AliasDictionary, works_with_empty_file)
{
    QFile db(QString::fromStdString(db_filename()));

    db.open(QIODevice::ReadWrite); // Create the database file.

    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_empty_database)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "{\n}\n");

    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_unexisting_file)
{
    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_broken_file)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "broken file {]");

    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, skips_correctly_broken_entries)
{
    std::string file_contents{"{\n"
                              "    \"alias1\": {\n"
                              "        \"command\": \"first_command\",\n"
                              "        \"instance\": \"first_instance\"\n"
                              "    },\n"
                              "    \"empty_entry\": {\n"
                              "    },\n"
                              "    \"alias2\": {\n"
                              "        \"command\": \"second_command\",\n"
                              "        \"instance\": \"second_instance\"\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    mp::AliasDict dict;
    ASSERT_EQ(dict.size(), 2u);

    auto a1 = dict.get_alias("alias1");
    ASSERT_TRUE(a1);
    ASSERT_EQ(a1->instance, "first_instance");
    ASSERT_EQ(a1->command, "first_command");

    auto a2 = dict.get_alias("alias2");
    ASSERT_TRUE(a2);
    ASSERT_EQ(a2->instance, "second_instance");
    ASSERT_EQ(a2->command, "second_command");
}

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

struct WriteReadTeststuite : public AliasDictionary, public WithParamInterface<AliasesVector>
{
};

TEST_P(WriteReadTeststuite, writes_and_reads_files)
{
    auto aliases_vector = GetParam();

    populate_db_file(aliases_vector);

    mp::AliasDict reader;

    for (const auto& alias : aliases_vector)
    {
        auto read_value = reader.get_alias(alias.first);
        ASSERT_TRUE(read_value);
        ASSERT_EQ((*read_value).instance, alias.second.instance);
        ASSERT_EQ((*read_value).command, alias.second.command);
    }

    // We test with this const/non-const iterators and size().
    ASSERT_EQ((size_t)std::distance(reader.cbegin(), reader.cend()), (size_t)aliases_vector.size());
    ASSERT_EQ((size_t)std::distance(reader.begin(), reader.end()), (size_t)aliases_vector.size());
    ASSERT_EQ(reader.size(), aliases_vector.size());
}

INSTANTIATE_TEST_SUITE_P(AliasDictionary, WriteReadTeststuite,
                         Values(AliasesVector{}, AliasesVector{{"w", {"fake", "w"}}},
                                AliasesVector{{"ipf", {"instance", "ip"}}},
                                AliasesVector{{"lsp", {"primary", "ls"}}, {"llp", {"primary", "ls"}}}));

TEST_F(AliasDictionary, correctly_removes_alias)
{
    mp::AliasDict dict;

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command"});
    ASSERT_FALSE(dict.empty());

    ASSERT_TRUE(dict.remove_alias("alias"));
    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_when_removing_unexisting_alias)
{
    mp::AliasDict dict;

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command"});
    ASSERT_FALSE(dict.empty());

    ASSERT_FALSE(dict.remove_alias("unexisting"));
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, correctly_gets_alias)
{
    mp::AliasDict dict;
    std::string alias_name{"alias"};
    mp::AliasDefinition alias_def{"instance", "command"};

    dict.add_alias(alias_name, alias_def);
    ASSERT_FALSE(dict.empty());

    auto result = dict.get_alias(alias_name);
    ASSERT_EQ(result, alias_def);
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, get_unexisting_alias_returns_nullopt)
{
    mp::AliasDict dict;

    ASSERT_EQ(dict.get_alias("unexisting"), mp::nullopt);
}

TEST_F(AliasDictionary, creates_backup_db)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command"}}});

    QString bak_filename = QString::fromStdString(db_filename() + ".bak");
    ASSERT_FALSE(QFile::exists(bak_filename));

    populate_db_file(AliasesVector{{"another_alias", {"an_instance", "a_command"}}});
    ASSERT_TRUE(QFile::exists(bak_filename));
}

struct FormatterTeststuite
    : public AliasDictionary,
      public WithParamInterface<std::tuple<AliasesVector, std::string, std::string, std::string, std::string>>
{
};

TEST_P(FormatterTeststuite, table)
{
    auto [aliases, csv_output, json_output, table_output, yaml_output] = GetParam();

    mp::AliasDict dict;

    for (const auto& alias : aliases)
        dict.add_alias(alias.first, alias.second);

    ASSERT_EQ(mp::ClientFormatter("csv").format(dict), csv_output);
    ASSERT_EQ(mp::ClientFormatter("json").format(dict), json_output);
    ASSERT_EQ(mp::ClientFormatter().format(dict), table_output);
    ASSERT_EQ(mp::ClientFormatter("table").format(dict), table_output);
    ASSERT_EQ(mp::ClientFormatter("yaml").format(dict), yaml_output);
}

std::string csv_head{"Alias,Instance,Command\n"};

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, FormatterTeststuite,
    Values(
        std::make_tuple(AliasesVector{}, csv_head, "{\n    \"aliases\": [\n    ]\n}\n", "No aliases defined.\n", "\n"),
        std::make_tuple(
            AliasesVector{{"lsp", {"primary", "ls"}}, {"llp", {"primary", "ls"}}},
            csv_head + "llp,primary,ls\nlsp,primary,ls\n",
            "{\n    \"aliases\": [\n        {\n"
            "            \"command\": \"ls\",\n            \"instance\": \"primary\",\n            \"name\": \"llp\"\n"
            "        },\n"
            "        {\n"
            "            \"command\": \"ls\",\n"
            "            \"instance\": \"primary\",\n            \"name\": \"lsp\"\n        }\n    ]\n}\n",
            "Alias  Instance  Command\nllp    primary   ls\nlsp    primary   ls\n",
            "llp:\n  - name: llp\n    instance: primary\n    command: ls\n"
            "lsp:\n  - name: lsp\n    instance: primary\n    command: ls\n")));
} // namespace
