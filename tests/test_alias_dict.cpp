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
#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>

#include "common.h"
#include "daemon_test_fixture.h"
#include "fake_alias_config.h"
#include "file_operations.h"
#include "json_utils.h"
#include "mock_file_ops.h"
#include "mock_platform.h"
#include "mock_vm_image_vault.h"
#include "stub_terminal.h"

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

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_empty_database)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "{\n}\n");

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_unexisting_file)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_broken_file)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "broken file {]");

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

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

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

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

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict reader(&trash_term);

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
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command"});
    ASSERT_FALSE(dict.empty());

    ASSERT_TRUE(dict.remove_alias("alias"));
    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_when_removing_unexisting_alias)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command"});
    ASSERT_FALSE(dict.empty());

    ASSERT_FALSE(dict.remove_alias("unexisting"));
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, correctly_gets_alias)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

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
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

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

TEST_F(AliasDictionary, throws_when_open_alias_file_fails)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(_)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    MP_ASSERT_THROW_THAT(mp::AliasDict dict(&trash_term), std::runtime_error,
                         mpt::match_what(HasSubstr("Error opening file '")));
}

struct FormatterTeststuite
    : public AliasDictionary,
      public WithParamInterface<std::tuple<AliasesVector, std::string, std::string, std::string, std::string>>
{
};

TEST_P(FormatterTeststuite, table)
{
    auto [aliases, csv_output, json_output, table_output, yaml_output] = GetParam();

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    for (const auto& alias : aliases)
        dict.add_alias(alias.first, alias.second);

    ASSERT_EQ(mp::CSVFormatter().format(dict), csv_output);
    ASSERT_EQ(mp::JsonFormatter().format(dict), json_output);
    ASSERT_EQ(mp::TableFormatter().format(dict), table_output);
    ASSERT_EQ(mp::YamlFormatter().format(dict), yaml_output);
}

std::string csv_head{"Alias,Instance,Command\n"};

INSTANTIATE_TEST_SUITE_P(AliasDictionary, FormatterTeststuite,
                         Values(std::make_tuple(AliasesVector{}, csv_head, "{\n    \"aliases\": [\n    ]\n}\n",
                                                "No aliases defined.\n", "aliases: ~\n"),
                                std::make_tuple(AliasesVector{{"lsp", {"primary", "ls"}}, {"llp", {"primary", "ls"}}},
                                                csv_head + "llp,primary,ls\nlsp,primary,ls\n",
                                                "{\n    \"aliases\": [\n        {\n"
                                                "            \"alias\": \"llp\",\n"
                                                "            \"command\": \"ls\",\n"
                                                "            \"instance\": \"primary\"\n"
                                                "        },\n"
                                                "        {\n"
                                                "            \"alias\": \"lsp\",\n"
                                                "            \"command\": \"ls\",\n"
                                                "            \"instance\": \"primary\"\n"
                                                "        }\n    ]\n}\n",
                                                "Alias  Instance  Command\nllp    primary   ls\nlsp    primary   ls\n",
                                                "aliases:\n  - alias: llp\n    command: ls\n    instance: primary\n"
                                                "  - alias: lsp\n    command: ls\n    instance: primary\n")));

struct RemoveInstanceTestsuite : public AliasDictionary,
                                 public WithParamInterface<std::pair<AliasesVector, std::vector<std::string>>>
{
};

TEST_P(RemoveInstanceTestsuite, removes_instance_aliases)
{
    auto [original_aliases, remaining_aliases] = GetParam();

    populate_db_file(original_aliases);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.remove_aliases_for_instance("instance_to_remove");

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

struct DaemonAliasTestsuite
    : public mpt::DaemonTestFixture,
      public FakeAliasConfig,
      public WithParamInterface<std::tuple<CmdList, std::string, std::vector<std::string> /* removed aliases */,
                                           std::vector<std::string> /* failed removal aliases */>>
{
};

TEST_P(DaemonAliasTestsuite, purge_removes_purged_instance_aliases_and_scripts)
{
    auto [commands, expected_output, expected_removed_aliases, expected_failed_removal] = GetParam();

    auto mock_image_vault = std::make_unique<NaggyMock<mpt::MockVMImageVault>>();

    EXPECT_CALL(*mock_image_vault, remove(_)).WillRepeatedly(Return());
    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _)).WillRepeatedly(Return(mp::VMImage{}));
    EXPECT_CALL(*mock_image_vault, prune_expired_images()).WillRepeatedly(Return());
    EXPECT_CALL(*mock_image_vault, has_record_for(_)).WillRepeatedly(Return(true));

    config_builder.vault = std::move(mock_image_vault);

    std::string json_contents = make_instance_json(mp::nullopt, {}, {"primary"});

    AliasesVector fake_aliases{{"lsp", {"primary", "ls"}}, {"lsz", {"real-zebraphant", "ls"}}};

    populate_db_file(fake_aliases);

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    ON_CALL(*mock_platform, create_alias_script(_, _)).WillByDefault(Return());
    for (const auto& removed_alias : expected_removed_aliases)
        EXPECT_CALL(*mock_platform, remove_alias_script(removed_alias));
    for (const auto& removed_alias : expected_failed_removal)
        EXPECT_CALL(*mock_platform, remove_alias_script(removed_alias)).WillOnce(Throw(std::runtime_error("foo")));

    mpt::TempDir temp_dir;
    QString filename(temp_dir.path() + "/multipassd-vm-instances.json");

    mpt::make_file_with_content(filename, json_contents);

    // Make the daemon look for the JSON on our temporary directory. It will read the contents of the file.
    config_builder.data_directory = temp_dir.path();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout, cerr;
    for (const auto& command : commands)
        send_command(command, cout, cerr);

    for (const auto& removed_alias : expected_failed_removal)
        EXPECT_THAT(cerr.str(),
                    HasSubstr(fmt::format("Warning: 'foo' when removing alias script for {}\n", removed_alias)));

    send_command({"aliases", "--format", "csv"}, cout);
    EXPECT_EQ(cout.str(), expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, DaemonAliasTestsuite,
    Values(std::make_tuple(CmdList{{"delete", "real-zebraphant"}, {"purge"}},
                           std::string{"Alias,Instance,Command\nlsp,primary,ls\n"}, std::vector<std::string>{"lsz"},
                           std::vector<std::string>{}),
           std::make_tuple(CmdList{{"delete", "--purge", "real-zebraphant"}},
                           std::string{"Alias,Instance,Command\nlsp,primary,ls\n"}, std::vector<std::string>{"lsz"},
                           std::vector<std::string>{}),
           std::make_tuple(CmdList{{"delete", "primary"}, {"delete", "primary", "real-zebraphant", "--purge"}},
                           std::string{"Alias,Instance,Command\n"}, std::vector<std::string>{"lsp", "lsz"},
                           std::vector<std::string>{}),
           std::make_tuple(CmdList{{"delete", "primary"}, {"delete", "primary", "real-zebraphant", "--purge"}},
                           std::string{"Alias,Instance,Command\n"}, std::vector<std::string>{},
                           std::vector<std::string>{"lsp", "lsz"}),
           std::make_tuple(CmdList{{"delete", "primary"}, {"delete", "primary", "real-zebraphant", "--purge"}},
                           std::string{"Alias,Instance,Command\n"}, std::vector<std::string>{"lsp"},
                           std::vector<std::string>{"lsz"}),
           std::make_tuple(CmdList{{"delete", "real-zebraphant"}, {"purge"}},
                           std::string{"Alias,Instance,Command\nlsp,primary,ls\n"}, std::vector<std::string>{},
                           std::vector<std::string>{"lsz"}),
           std::make_tuple(CmdList{{"delete", "real-zebraphant", "primary"}, {"purge"}},
                           std::string{"Alias,Instance,Command\n"}, std::vector<std::string>{},
                           std::vector<std::string>{"lsz", "lsp"})));
} // namespace
