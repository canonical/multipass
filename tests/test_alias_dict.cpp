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

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/csv_formatter.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>
#include <multipass/cli/yaml_formatter.h>
#include <multipass/constants.h>

#include "common.h"
#include "daemon_test_fixture.h"
#include "fake_alias_config.h"
#include "file_operations.h"
#include "json_test_utils.h"
#include "mock_file_ops.h"
#include "mock_platform.h"
#include "mock_settings.h"
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

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.get_active_context().size(), 0u);
}

TEST_F(AliasDictionary, works_with_empty_database)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "{\n}\n");

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.get_active_context().size(), 0u);
}

TEST_F(AliasDictionary, works_with_unexisting_file)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.get_active_context().size(), 0u);
}

TEST_F(AliasDictionary, works_with_broken_file)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "broken file {]");

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.get_active_context().size(), 0u);
}

TEST_F(AliasDictionary, SkipsCorrectlyBrokenEntriesOldFormat)
{
    std::string file_contents{"{\n"
                              "    \"alias1\": {\n"
                              "        \"command\": \"first_command\",\n"
                              "        \"instance\": \"first_instance\",\n"
                              "        \"working-directory\": \"map\"\n"
                              "    },\n"
                              "    \"empty_entry\": {\n"
                              "    },\n"
                              "    \"alias2\": {\n"
                              "        \"command\": \"second_command\",\n"
                              "        \"instance\": \"second_instance\",\n"
                              "        \"working-directory\": \"default\"\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 2u);

    auto a1 = dict.get_alias("alias1");
    ASSERT_TRUE(a1);
    ASSERT_EQ(a1->instance, "first_instance");
    ASSERT_EQ(a1->command, "first_command");
    ASSERT_EQ(a1->working_directory, "map");

    auto a2 = dict.get_alias("alias2");
    ASSERT_TRUE(a2);
    ASSERT_EQ(a2->instance, "second_instance");
    ASSERT_EQ(a2->command, "second_command");
    ASSERT_EQ(a2->working_directory, "default");
}

TEST_F(AliasDictionary, SkipsCorrectlyBrokenEntries)
{
    std::string file_contents{"{\n"
                              "    \"active-context\": \"default\",\n"
                              "    \"contexts\": {\n"
                              "        \"default\": {\n"
                              "            \"alias1\": {\n"
                              "                \"command\": \"first_command\",\n"
                              "                \"instance\": \"first_instance\",\n"
                              "                \"working-directory\": \"map\"\n"
                              "            },\n"
                              "            \"empty_entry\": {\n"
                              "            },\n"
                              "            \"alias2\": {\n"
                              "                \"command\": \"second_command\",\n"
                              "                \"instance\": \"second_instance\",\n"
                              "                \"working-directory\": \"default\"\n"
                              "            }\n"
                              "        }\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 2u);

    auto a1 = dict.get_alias("alias1");
    ASSERT_TRUE(a1);
    ASSERT_EQ(a1->instance, "first_instance");
    ASSERT_EQ(a1->command, "first_command");
    ASSERT_EQ(a1->working_directory, "map");

    auto a2 = dict.get_alias("alias2");
    ASSERT_TRUE(a2);
    ASSERT_EQ(a2->instance, "second_instance");
    ASSERT_EQ(a2->command, "second_command");
    ASSERT_EQ(a2->working_directory, "default");
}

// In old versions, the file did not contain the `working-directory` flag in the JSON, because the
// `--no-working-directory` flag were not yet introduced. In case the file was generated by an old version,
// the flag must be `false`, in order to maintain backwards compatibility.
TEST_F(AliasDictionary, mapDirMissingTranslatesToDefault)
{
    std::string file_contents{"{\n"
                              "    \"alias3\": {\n"
                              "        \"command\": \"third_command\",\n"
                              "        \"instance\": \"third_instance\"\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    auto a3 = dict.get_alias("alias3");
    ASSERT_TRUE(a3);
    ASSERT_EQ(a3->instance, "third_instance");
    ASSERT_EQ(a3->command, "third_command");
    ASSERT_EQ(a3->working_directory, "default");
}

TEST_F(AliasDictionary, mapDirEmptyStringTranslatesToDefault)
{
    std::string file_contents{"{\n"
                              "    \"alias4\": {\n"
                              "        \"command\": \"fourth_command\",\n"
                              "        \"instance\": \"fourth_instance\",\n"
                              "        \"working-directory\": \"\"\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    auto a4 = dict.get_alias("alias4");
    ASSERT_TRUE(a4);
    ASSERT_EQ(a4->instance, "fourth_instance");
    ASSERT_EQ(a4->command, "fourth_command");
    ASSERT_EQ(a4->working_directory, "default");
}

TEST_F(AliasDictionary, mapDirWrongThrows)
{
    std::string file_contents{"{\n"
                              "    \"alias5\": {\n"
                              "        \"command\": \"fifth_command\",\n"
                              "        \"instance\": \"fifth_instance\",\n"
                              "        \"working-directory\": \"wrong string\"\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);

    MP_ASSERT_THROW_THAT(mp::AliasDict dict(&trash_term), std::runtime_error,
                         mpt::match_what(HasSubstr("invalid working_directory string \"wrong string\"")));
}

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

struct WriteReadTestsuite : public AliasDictionary, public WithParamInterface<AliasesVector>
{
};

TEST_P(WriteReadTestsuite, writes_and_reads_files)
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
    ASSERT_EQ((size_t)std::distance(reader.get_active_context().cbegin(), reader.get_active_context().cend()),
              (size_t)aliases_vector.size());
    ASSERT_EQ((size_t)std::distance(reader.get_active_context().begin(), reader.get_active_context().end()),
              (size_t)aliases_vector.size());
    ASSERT_EQ(reader.get_active_context().size(), aliases_vector.size());
}

INSTANTIATE_TEST_SUITE_P(AliasDictionary, WriteReadTestsuite,
                         Values(AliasesVector{}, AliasesVector{{"w", {"fake", "w", "map"}}},
                                AliasesVector{{"ipf", {"instance", "ip", "map"}}},
                                AliasesVector{{"lsp", {"primary", "ls", "map"}}, {"llp", {"primary", "ls", "map"}}}));

TEST_F(AliasDictionary, addAliasWorks)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_TRUE(dict.add_alias("repeated", mp::AliasDefinition{"instance-1", "command-1", "map"}));

    ASSERT_FALSE(dict.add_alias("repeated", mp::AliasDefinition{"instance-2", "command-2", "map"}));

    ASSERT_EQ(*dict.get_alias("repeated"), (mp::AliasDefinition{"instance-1", "command-1", "map"}));
}

TEST_F(AliasDictionary, existsAliasWorksWithExistingAlias)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("existing", mp::AliasDefinition{"instance", "command", "map"});

    ASSERT_TRUE(dict.exists_alias("existing"));
}

TEST_F(AliasDictionary, existsAliasWorksWithUnexistingAlias)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_FALSE(dict.exists_alias("unexisting"));
}

TEST_F(AliasDictionary, correctly_removes_alias)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.active_context_name(), "default");
    dict.add_alias("alias", mp::AliasDefinition{"instance", "command", "map"});
    ASSERT_FALSE(dict.empty());

    ASSERT_TRUE(dict.remove_alias("alias"));
    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.get_active_context().size(), 0u);
}

TEST_F(AliasDictionary, works_when_removing_unexisting_alias)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("alias", mp::AliasDefinition{"instance", "command", "map"});
    ASSERT_EQ(dict.size(), 1u);
    ASSERT_FALSE(dict.get_active_context().empty());

    ASSERT_FALSE(dict.remove_alias("unexisting"));
    ASSERT_EQ(dict.size(), 1u);
    ASSERT_FALSE(dict.get_active_context().empty());
}

TEST_F(AliasDictionary, clearWorks)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first", mp::AliasDefinition{"instance", "command", "default"});
    dict.add_alias("second", mp::AliasDefinition{"other_instance", "other_command", "map"});
    dict.clear();

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, correctlyGetsAliasInDefaultContext)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    std::string alias_name{"alias"};
    mp::AliasDefinition alias_def{"instance", "command", "map"};

    dict.add_alias(alias_name, alias_def);
    ASSERT_FALSE(dict.empty());

    auto result = dict.get_alias(alias_name);
    ASSERT_EQ(result, alias_def);
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, correctlyGetsUniqueAliasInAnotherContext)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    std::string alias_name{"alias"};
    mp::AliasDefinition alias_def{"instance", "command", "map"};

    dict.add_alias(alias_name, alias_def);

    dict.set_active_context("new_context");

    auto result = dict.get_alias(alias_name);
    ASSERT_EQ(result, alias_def);
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, correctlyGetsAliasInNonDefaultContext)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    std::string context{"non-default"};
    std::string alias_name{"alias"};
    mp::AliasDefinition alias_def{"instance", "command", "map"};

    dict.set_active_context(context);
    dict.add_alias(alias_name, alias_def);
    dict.set_active_context("default");

    auto result = dict.get_alias(context + '.' + alias_name);
    ASSERT_EQ(result, alias_def);
    ASSERT_FALSE(dict.empty());
}

TEST_F(AliasDictionary, get_unexisting_alias_returns_nullopt)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.get_alias("unexisting"), std::nullopt);
}

TEST_F(AliasDictionary, throws_when_open_alias_file_fails)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    MP_ASSERT_THROW_THAT(mp::AliasDict dict(&trash_term), std::runtime_error,
                         mpt::match_what(HasSubstr("Error opening file '")));
}

struct FormatterTestsuite
    : public AliasDictionary,
      public WithParamInterface<
          std::tuple<std::string, AliasesVector, std::string, std::string, std::string, std::string>>
{
};

TEST_P(FormatterTestsuite, table)
{
    auto [context, aliases, csv_output, json_output, table_output, yaml_output] = GetParam();

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.set_active_context(context);

    for (const auto& alias : aliases)
        dict.add_alias(alias.first, alias.second);

    dict.set_active_context("default");

    ASSERT_EQ(mp::CSVFormatter().format(dict), csv_output);
    ASSERT_EQ(mp::JsonFormatter().format(dict), json_output);
    ASSERT_EQ(mp::TableFormatter().format(dict), table_output);
    ASSERT_EQ(mp::YamlFormatter().format(dict), yaml_output);
}

const std::string csv_head{"Alias,Instance,Command,Working directory,Context\n"};

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary,
    FormatterTestsuite,
    Values(std::make_tuple("default",
                           AliasesVector{},
                           csv_head,
                           "{\n"
                           "    \"active-context\": \"default\",\n"
                           "    \"contexts\": {\n"
                           "        \"default\": {\n"
                           "        }\n"
                           "    }\n"
                           "}\n",
                           "No aliases defined.\n",
                           "active_context: default\n"
                           "aliases:\n"
                           "  default: ~\n"),
           std::make_tuple("default",
                           AliasesVector{{"lsp", {"primary", "ls", "map"}}, {"llp", {"primary", "ls", "map"}}},
                           csv_head + "llp,primary,ls,map,default*\n"
                                      "lsp,primary,ls,map,default*\n",
                           "{\n"
                           "    \"active-context\": \"default\",\n"
                           "    \"contexts\": {\n"
                           "        \"default\": {\n"
                           "            \"llp\": {\n"
                           "                \"command\": \"ls\",\n"
                           "                \"instance\": \"primary\",\n"
                           "                \"working-directory\": \"map\"\n"
                           "            },\n"
                           "            \"lsp\": {\n"
                           "                \"command\": \"ls\",\n"
                           "                \"instance\": \"primary\",\n"
                           "                \"working-directory\": \"map\"\n"
                           "            }\n"
                           "        }\n"
                           "    }\n"
                           "}\n",
                           "Alias   Instance   Command   Context    Working directory\n"
                           "llp     primary    ls        default*   map\n"
                           "lsp     primary    ls        default*   map\n",
                           "active_context: default\n"
                           "aliases:\n"
                           "  default:\n"
                           "    - alias: llp\n"
                           "      command: ls\n"
                           "      instance: primary\n"
                           "      working-directory: map\n"
                           "    - alias: lsp\n"
                           "      command: ls\n"
                           "      instance: primary\n"
                           "      working-directory: map\n"),
           std::make_tuple("docker",
                           AliasesVector{{"docker", {"docker", "docker", "map"}},
                                         {"docker-compose", {"docker", "docker-compose", "map"}}},
                           csv_head + "docker,docker,docker,map,docker\n"
                                      "docker-compose,docker,docker-compose,map,docker\n",
                           "{\n"
                           "    \"active-context\": \"default\",\n"
                           "    \"contexts\": {\n"
                           "        \"default\": {\n"
                           "        },\n"
                           "        \"docker\": {\n"
                           "            \"docker\": {\n"
                           "                \"command\": \"docker\",\n"
                           "                \"instance\": \"docker\",\n"
                           "                \"working-directory\": \"map\"\n"
                           "            },\n"
                           "            \"docker-compose\": {\n"
                           "                \"command\": \"docker-compose\",\n"
                           "                \"instance\": \"docker\",\n"
                           "                \"working-directory\": \"map\"\n"
                           "            }\n"
                           "        }\n"
                           "    }\n"
                           "}\n",
                           "Alias            Instance   Command          Context   Working directory\n"
                           "docker           docker     docker           docker    map\n"
                           "docker-compose   docker     docker-compose   docker    map\n",
                           "active_context: default\n"
                           "aliases:\n"
                           "  default: ~\n"
                           "  docker:\n"
                           "    - alias: docker\n"
                           "      command: docker\n"
                           "      instance: docker\n"
                           "      working-directory: map\n"
                           "    - alias: docker-compose\n"
                           "      command: docker-compose\n"
                           "      instance: docker\n"
                           "      working-directory: map\n")));

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

    ASSERT_EQ(dict.get_active_context().size(), remaining_aliases.size());

    for (auto remaining_alias : remaining_aliases)
        ASSERT_TRUE(dict.get_alias(remaining_alias));
}

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, RemoveInstanceTestsuite,
    Values(std::make_pair(AliasesVector{{"some_alias", {"instance_to_remove", "some_command", "map"}},
                                        {"other_alias", {"other_instance", "other_command", "map"}},
                                        {"another_alias", {"instance_to_remove", "another_command", "map"}},
                                        {"yet_another_alias", {"yet_another_instance", "yet_another_command", "map"}}},
                          std::vector<std::string>{"other_alias", "yet_another_alias"}),
           std::make_pair(AliasesVector{{"alias", {"instance", "command", "map"}}}, std::vector<std::string>{"alias"}),
           std::make_pair(AliasesVector{{"alias", {"instance_to_remove", "command", "map"}}},
                          std::vector<std::string>{})));

typedef std::vector<std::vector<std::string>> CmdList;

struct DaemonAliasTestsuite
    : public mpt::DaemonTestFixture,
      public FakeAliasConfig,
      public WithParamInterface<std::tuple<CmdList, std::string, std::vector<std::string> /* removed aliases */,
                                           std::vector<std::string> /* failed removal aliases */>>
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillRepeatedly(Return("none"));
    }

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_P(DaemonAliasTestsuite, purge_removes_purged_instance_aliases_and_scripts)
{
    auto [commands, expected_output, expected_removed_aliases, expected_failed_removal] = GetParam();

    auto mock_image_vault = std::make_unique<NaggyMock<mpt::MockVMImageVault>>();

    EXPECT_CALL(*mock_image_vault, remove(_)).WillRepeatedly(Return());
    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillRepeatedly(Return(mp::VMImage{}));
    EXPECT_CALL(*mock_image_vault, prune_expired_images()).WillRepeatedly(Return());
    EXPECT_CALL(*mock_image_vault, has_record_for(_)).WillRepeatedly(Return(true));

    config_builder.vault = std::move(mock_image_vault);
    auto mock_factory = use_a_mock_vm_factory();

    EXPECT_CALL(*mock_factory, remove_resources_for(_)).WillRepeatedly(Return());

    std::string json_contents = make_instance_json(std::nullopt, {}, {"primary"});

    AliasesVector fake_aliases{{"lsp", {"primary", "ls", "map"}}, {"lsz", {"real-zebraphant", "ls", "map"}}};

    populate_db_file(fake_aliases);

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    ON_CALL(*mock_platform, create_alias_script(_, _)).WillByDefault(Return());

    for (const auto& removed_alias : expected_removed_aliases)
    {
        EXPECT_CALL(*mock_platform, remove_alias_script("default." + removed_alias)).Times(1);
        EXPECT_CALL(*mock_platform, remove_alias_script(removed_alias)).Times(1);
    }

    for (const auto& removed_alias : expected_failed_removal)
    {
        EXPECT_CALL(*mock_platform, remove_alias_script("default." + removed_alias))
            .WillOnce(Throw(std::runtime_error("foo")));
    }

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
        EXPECT_THAT(cerr.str(), HasSubstr(fmt::format("Warning: 'foo' when removing alias script for default.{}\n",
                                                      removed_alias)));

    send_command({"aliases", "--format", "csv"}, cout);
    EXPECT_EQ(cout.str(), expected_output);
}

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, DaemonAliasTestsuite,
    Values(
        std::make_tuple(CmdList{{"delete", "real-zebraphant"}, {"purge"}}, csv_head + "lsp,primary,ls,map,default*\n",
                        std::vector<std::string>{"lsz"}, std::vector<std::string>{}),
        std::make_tuple(CmdList{{"delete", "--purge", "real-zebraphant"}}, csv_head + "lsp,primary,ls,map,default*\n",
                        std::vector<std::string>{"lsz"}, std::vector<std::string>{}),
        std::make_tuple(CmdList{{"delete", "primary"}, {"delete", "primary", "real-zebraphant", "--purge"}}, csv_head,
                        std::vector<std::string>{"lsp", "lsz"}, std::vector<std::string>{}),
        std::make_tuple(CmdList{{"delete", "primary"}, {"delete", "primary", "real-zebraphant", "--purge"}}, csv_head,
                        std::vector<std::string>{}, std::vector<std::string>{"lsp", "lsz"}),
        std::make_tuple(CmdList{{"delete", "primary"}, {"delete", "primary", "real-zebraphant", "--purge"}}, csv_head,
                        std::vector<std::string>{"lsp"}, std::vector<std::string>{"lsz"}),
        std::make_tuple(CmdList{{"delete", "real-zebraphant"}, {"purge"}}, csv_head + "lsp,primary,ls,map,default*\n",
                        std::vector<std::string>{}, std::vector<std::string>{"lsz"}),
        std::make_tuple(CmdList{{"delete", "real-zebraphant", "primary"}, {"purge"}}, csv_head,
                        std::vector<std::string>{}, std::vector<std::string>{"lsz", "lsp"})));

TEST_F(AliasDictionary, unexistingActiveContextThrows)
{
    std::string file_contents{"{\n"
                              "    \"active-context\": \"inconsistent\",\n"
                              "    \"contexts\": {\n"
                              "        \"default\": {\n"
                              "        }\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.active_context_name(), "inconsistent");
    MP_ASSERT_THROW_THAT(dict.get_active_context(), std::runtime_error,
                         mpt::match_what(HasSubstr("active context \"inconsistent\" does not exist in dictionary")));
}

TEST_F(AliasDictionary, removeContextWorks)
{
    std::string file_contents{"{\n"
                              "    \"active-context\": \"default\",\n"
                              "    \"contexts\": {\n"
                              "        \"default\": {\n"
                              "            \"alias1\": {\n"
                              "                \"command\": \"first_command\",\n"
                              "                \"instance\": \"first_instance\",\n"
                              "                \"working-directory\": \"map\"\n"
                              "            }\n"
                              "        },\n"
                              "        \"another\": {\n"
                              "            \"alias2\": {\n"
                              "                \"command\": \"second_command\",\n"
                              "                \"instance\": \"second_instance\",\n"
                              "                \"working-directory\": \"default\"\n"
                              "            }\n"
                              "        }\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 2u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 1u);

    dict.set_active_context("another");
    ASSERT_EQ(dict.active_context_name(), "another");
    ASSERT_EQ(dict.get_active_context().size(), 1u);
    auto a2 = dict.get_alias("alias2");
    ASSERT_TRUE(a2);

    dict.remove_context("another");
    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.active_context_name(), "default");

    dict.set_active_context("another");
    ASSERT_EQ(dict.get_active_context().size(), 0u);
}

TEST_F(AliasDictionary, removeDefaultContextWorks)
{
    std::string file_contents{"{\n"
                              "    \"active-context\": \"default\",\n"
                              "    \"contexts\": {\n"
                              "        \"default\": {\n"
                              "            \"alias1\": {\n"
                              "                \"command\": \"first_command\",\n"
                              "                \"instance\": \"first_instance\",\n"
                              "                \"working-directory\": \"map\"\n"
                              "            }\n"
                              "        },\n"
                              "        \"another\": {\n"
                              "            \"alias2\": {\n"
                              "                \"command\": \"second_command\",\n"
                              "                \"instance\": \"second_instance\",\n"
                              "                \"working-directory\": \"default\"\n"
                              "            }\n"
                              "        }\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 2u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 1u);

    dict.set_active_context("another");
    ASSERT_EQ(dict.active_context_name(), "another");
    ASSERT_EQ(dict.get_active_context().size(), 1u);
    auto a2 = dict.get_alias("alias2");
    ASSERT_TRUE(a2);

    dict.set_active_context("default");
    ASSERT_EQ(dict.size(), 2u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 1u);

    dict.remove_context("default");

    // Removing the default context just empties it.
    ASSERT_EQ(dict.size(), 2u);
    ASSERT_EQ(dict.get_active_context().size(), 0u);

    dict.set_active_context("another");
    ASSERT_EQ(dict.get_active_context().size(), 1u);
}

TEST_F(AliasDictionary, removingUnexistingContextDoesNothing)
{
    std::string file_contents{"{\n"
                              "    \"active-context\": \"default\",\n"
                              "    \"contexts\": {\n"
                              "        \"default\": {\n"
                              "            \"alias1\": {\n"
                              "                \"command\": \"first_command\",\n"
                              "                \"instance\": \"first_instance\",\n"
                              "                \"working-directory\": \"map\"\n"
                              "            }\n"
                              "        }\n"
                              "    }\n"
                              "}\n"};

    mpt::make_file_with_content(QString::fromStdString(db_filename()), file_contents);

    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 1u);

    dict.remove_context("unexisting");
    ASSERT_EQ(dict.size(), 1u);
    ASSERT_EQ(dict.active_context_name(), "default");
    ASSERT_EQ(dict.get_active_context().size(), 1u);
}

TEST_F(AliasDictionary, unqualifiedGetContextAndAliasWorksIfInDifferentContext)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first_alias", mp::AliasDefinition{"instance-1", "command-1", "map"});
    dict.set_active_context("new_context");

    ASSERT_EQ(dict.get_context_and_alias("first_alias"), std::nullopt);
}

TEST_F(AliasDictionary, unqualifiedGetContextAndAliasWorksIfInCurrentContext)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first_alias", mp::AliasDefinition{"instance-1", "command-1", "map"});
    auto context_and_alias = dict.get_context_and_alias("first_alias");

    ASSERT_EQ(context_and_alias->first, "default");
    ASSERT_EQ(context_and_alias->second, "first_alias");
}

TEST_F(AliasDictionary, unqualifiedGetContextAndAliasWorksWithEquallyNamesAliasesInDifferentContext)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first_alias", mp::AliasDefinition{"instance-1", "command-1", "map"});
    dict.set_active_context("new_context");
    dict.add_alias("first_alias", mp::AliasDefinition{"instance-2", "command-2", "map"});
    auto context_and_alias = dict.get_context_and_alias("first_alias");

    ASSERT_EQ(context_and_alias->first, "new_context");
    ASSERT_EQ(context_and_alias->second, "first_alias");
}

TEST_F(AliasDictionary, qualifiedGetContextAndAliasWorksIfAliasAndContextExist)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first_alias", mp::AliasDefinition{"instance-1", "command-1", "map"});
    dict.set_active_context("new_context");
    dict.add_alias("second_alias", mp::AliasDefinition{"instance-2", "command-2", "map"});
    auto context_and_alias = dict.get_context_and_alias("default.first_alias");

    ASSERT_EQ(context_and_alias->first, "default");
    ASSERT_EQ(context_and_alias->second, "first_alias");
}

TEST_F(AliasDictionary, qualifiedGetContextAndAliasWorksIfContextDoesNotExist)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first_alias", mp::AliasDefinition{"instance-1", "command-1", "map"});
    ASSERT_EQ(dict.get_context_and_alias("nonexistent_context.first_alias"), std::nullopt);
}

TEST_F(AliasDictionary, qualifiedGetContextAndAliasWorksIfAliasDoesNotExist)
{
    std::stringstream trash_stream;
    mpt::StubTerminal trash_term(trash_stream, trash_stream, trash_stream);
    mp::AliasDict dict(&trash_term);

    dict.add_alias("first_alias", mp::AliasDefinition{"instance-1", "command-1", "map"});
    ASSERT_EQ(dict.get_context_and_alias("default.nonexistent_alias"), std::nullopt);
}

} // namespace
