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
#include <multipass/constants.h>

#include <gmock/gmock.h>

#include "file_operations.h"
#include "mock_standard_paths.h"
#include "temp_dir.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AliasDictionary : public Test
{
    AliasDictionary()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(temp_dir.path()));
    }

    std::string db_filename()
    {
        const auto file_name = QStringLiteral("%1/%1_aliases.json").arg(mp::client_name);

        return temp_dir.filePath(file_name).toStdString();
    }

    void populate_db_file(const std::vector<std::pair<std::string, mp::AliasDefinition>>& aliases)
    {
        mp::AliasDict writer;

        for (const auto& alias : aliases)
            writer.add_alias(alias.first, alias.second);
    }

    mpt::TempDir temp_dir;
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
                              "        \"arguments\": [\n"
                              "            \"-a\",\n"
                              "            \"-b\",\n"
                              "            \"-c\"\n"
                              "        ],\n"
                              "        \"command\": \"first_command\",\n"
                              "        \"instance\": \"first_instance\"\n"
                              "    },\n"
                              "    \"empty_entry\": {\n"
                              "    },\n"
                              "    \"alias2\": {\n"
                              "        \"arguments\": [\n"
                              "        ],\n"
                              "        \"command\": \"second_command\",\n"
                              "        \"instance\": \"second_instance\"\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    mp::AliasDict dict;
    ASSERT_EQ(std::distance(dict.cbegin(), dict.cend()), 2u);

    auto a1 = dict.get_alias("alias1");
    ASSERT_TRUE(a1);
    ASSERT_EQ(a1->instance, "first_instance");
    ASSERT_EQ(a1->command, "first_command");
    std::vector<std::string> first_arguments{"-a", "-b", "-c"};
    ASSERT_EQ(a1->arguments, first_arguments);

    auto a2 = dict.get_alias("alias2");
    ASSERT_TRUE(a2);
    ASSERT_EQ(a2->instance, "second_instance");
    ASSERT_EQ(a2->command, "second_command");
    ASSERT_TRUE(a2->arguments.empty());
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
        ASSERT_EQ((*read_value).arguments, alias.second.arguments);
    }

    ASSERT_EQ((size_t)std::distance(reader.cbegin(), reader.cend()), (size_t)aliases_vector.size());
    ASSERT_EQ((size_t)std::distance(reader.begin(), reader.end()), (size_t)aliases_vector.size());
}

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, WriteReadTeststuite,
    Values(AliasesVector{}, AliasesVector{{"w", {"fake", "w", {}}}}, AliasesVector{{"ipf", {"instance", "ip", {"a"}}}},
           AliasesVector{{"lsp", {"primary", "ls", {}}}, {"llp", {"primary", "ls", {"-l", "-a"}}}}));

TEST_F(AliasDictionary, correctly_removes_alias)
{
    mp::AliasDict dict;

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command", {"arg0", "arg1"}});
    ASSERT_FALSE(dict.empty());

    ASSERT_TRUE(dict.remove_alias("alias"));
    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_when_removing_unexisting_alias)
{
    mp::AliasDict dict;

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command", {"arg0", "arg1"}});
    ASSERT_FALSE(dict.empty());

    ASSERT_FALSE(dict.remove_alias("unexisting"));
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, correctly_gets_alias)
{
    mp::AliasDict dict;
    std::string alias_name{"alias"};
    mp::AliasDefinition alias_def{"instance", "command", {"arg0", "arg1"}};

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
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command", {}}}});

    QString bak_filename = QString::fromStdString(db_filename() + ".bak");
    ASSERT_FALSE(QFile::exists(bak_filename));

    populate_db_file(AliasesVector{{"another_alias", {"an_instance", "a_command", {}}}});
    ASSERT_TRUE(QFile::exists(bak_filename));
}

} // namespace
