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

#include "daemon_test_fixture.h"
#include "fake_alias_config.h"
#include "file_operations.h"
#include "json_utils.h"
#include "mock_vm_image_vault.h"

#include "src/client/common/client_formatter.h"
#include "src/daemon/daemon.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AliasDictionary : public FakeAliasConfig, public Test
{
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

struct RemoveInstanceTestsuite : public AliasDictionary,
                                 public WithParamInterface<std::pair<AliasesVector, std::vector<std::string>>>
{
};

TEST_P(RemoveInstanceTestsuite, removes_instance_aliases)
{
    auto [original_aliases, remaining_aliases] = GetParam();

    populate_db_file(original_aliases);

    mp::AliasDict dict;

    auto original_dict_size = dict.size();

    ASSERT_EQ(dict.remove_aliases_for_instance("instance_to_remove"), original_dict_size - remaining_aliases.size());

    ASSERT_EQ(dict.size(), remaining_aliases.size());

    for (auto remaining_alias : remaining_aliases)
        ASSERT_TRUE(dict.get_alias(remaining_alias));
}

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, RemoveInstanceTestsuite,
    Values(std::make_pair(AliasesVector{{"some_alias", {"instance_to_remove", "some_command"}},
                                        {"other_alias", {"other_instance", "other_command"}},
                                        {"another_alias", {"instance_to_remove", "another_command"}},
                                        {"yet_another_alias", {"yet_another_instance", "yet_another_command"}}},
                          std::vector<std::string>{"other_alias", "yet_another_alias"}),
           std::make_pair(AliasesVector{{"alias", {"instance", "command"}}}, std::vector<std::string>{"alias"}),
           std::make_pair(AliasesVector{{"alias", {"instance_to_remove", "command"}}}, std::vector<std::string>{})));

typedef std::vector<std::vector<std::string>> CmdList;

struct DaemonAliasTestsuite : public mpt::DaemonTestFixture,
                              public FakeAliasConfig,
                              public WithParamInterface<std::pair<CmdList, std::string>>
{
};

TEST_P(DaemonAliasTestsuite, purge_removes_purged_instance_aliases)
{
    auto [commands, expected_output] = GetParam();

    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    std::string json_contents = make_instance_json(mp::nullopt, {}, {"primary"});

    populate_db_file(AliasesVector{{"lsp", {"primary", "ls"}}, {"lsz", {"real-zebraphant", "ls"}}});

    mpt::TempDir temp_dir;
    QString filename(temp_dir.path() + "/multipassd-vm-instances.json");

    mpt::make_file_with_content(filename, json_contents);

    // Make the daemon look for the JSON on our temporary directory. It will read the contents of the file.
    config_builder.data_directory = temp_dir.path();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"list", "--format", "csv"}, stream);
    EXPECT_THAT(stream.str(), HasSubstr("primary"));
    EXPECT_THAT(stream.str(), HasSubstr("real-zebraphant"));

    stream.str({});
    send_command({"aliases", "--format", "csv"}, stream);
    EXPECT_EQ(stream.str(), "Alias,Instance,Command\nlsp,primary,ls\nlsz,real-zebraphant,ls\n");

    for (const auto& command : commands)
        send_command(command);

    stream.str({});
    send_command({"aliases", "--format", "csv"}, stream);
    EXPECT_EQ(stream.str(), expected_output);
}

INSTANTIATE_TEST_SUITE_P(AliasDictionary, DaemonAliasTestsuite,
                         Values(std::make_pair(CmdList{{"delete", "real-zebraphant"}, {"purge"}},
                                               std::string{"Alias,Instance,Command\nlsp,primary,ls\n"}),
                                std::make_pair(CmdList{{"delete", "--purge", "real-zebraphant"}},
                                               std::string{"Alias,Instance,Command\nlsp,primary,ls\n"}),
                                std::make_pair(CmdList{{"delete", "primary"},
                                                       {"delete", "primary", "real-zebraphant", "--purge"}},
                                               std::string{"Alias,Instance,Command\n"})));
} // namespace
