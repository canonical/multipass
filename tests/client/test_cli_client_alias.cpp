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

#include "client_test_fixture.h"

#include <tests/fake_alias_config.h>
#include <tests/mock_environment_helpers.h>
#include <tests/mock_file_ops.h>
#include <tests/mock_platform.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct ClientAlias : public mpt::ClientTestFixture, public FakeAliasConfig
{
    ClientAlias()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(fake_alias_dir.path()));

        EXPECT_CALL(*mock_platform, create_alias_script(_, _)).WillRepeatedly(Return());
        EXPECT_CALL(*mock_platform, remove_alias_script(_)).WillRepeatedly(Return());
    }

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject()};
    mpt::MockPlatform* mock_platform = attr.first;
};

auto info_function = [](grpc::ServerContext*, const mp::InfoRequest* request,
                        grpc::ServerWriter<mp::InfoReply>* response) {
    mp::InfoReply info_reply;

    if (request->instance_names().instance_name(0) == "primary")
    {
        auto vm_info = info_reply.add_info();
        vm_info->set_name("primary");
        vm_info->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);

        response->Write(info_reply);

        return grpc::Status{};
    }
    else
        return grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
};

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

struct AliasHelpTestSuite : public ClientAlias, public WithParamInterface<std::pair<std::string, std::string>>
{
};

struct ClientAliasNameSuite : public ClientAlias,
                              public WithParamInterface<std::tuple<std::string /* command */, std::string /* path */>>
{
};

struct AliasArgumentCheckTestSuite
    : public ClientAlias,
      public WithParamInterface<std::tuple<std::vector<std::string>, mp::ReturnCode, std::string, std::string>>
{
};
} // namespace

TEST_F(ClientAlias, alias_creates_alias)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}}});

    EXPECT_EQ(send_command({"alias", "primary:another_command", "another_alias"}), mp::ReturnCode::Ok);

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\nanother_alias,"
                                   "primary,another_command\n");
}

TEST_F(ClientAlias, fails_if_cannot_write_script)
{
    EXPECT_CALL(*mock_platform, create_alias_script(_, _)).Times(1).WillRepeatedly(Throw(std::runtime_error("aaa")));

    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"alias", "primary:command"}, trash_stream, cerr_stream), mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Error when creating script for alias: aaa\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\n");
}

TEST_F(ClientAlias, alias_does_not_overwrite_alias)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"alias", "primary:another_command", "an_alias"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Alias 'an_alias' already exists\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\n");
}

TEST_F(ClientAlias, empty_aliases)
{
    std::stringstream cout_stream;
    send_command({"aliases"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "No aliases defined.\n");
}

TEST_F(ClientAlias, bad_aliases_format)
{
    std::stringstream cerr_stream;
    send_command({"aliases", "--format", "wrong"}, trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), "Invalid format type given.\n");
}

TEST_F(ClientAlias, too_many_aliases_arguments)
{
    std::stringstream cerr_stream;
    send_command({"aliases", "bad_argument"}, trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), "This command takes no arguments\n");
}

TEST_F(ClientAlias, execute_existing_alias)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command"}}});

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));

    EXPECT_EQ(send_command({"some_alias"}), mp::ReturnCode::Ok);
}

TEST_F(ClientAlias, execute_unexisting_alias)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command"}}});

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).Times(0);

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"other_undefined_alias"}, cout_stream), mp::ReturnCode::CommandLineError);
    EXPECT_THAT(cout_stream.str(), HasSubstr("Unknown command or alias"));
}

TEST_F(ClientAlias, execute_alias_with_arguments)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command"}}});

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));

    EXPECT_EQ(send_command({"some_alias", "some_argument"}), mp::ReturnCode::Ok);
}

TEST_F(ClientAlias, fails_executing_alias_without_separator)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command"}}});

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).Times(0);

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"some_alias", "--some-option"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_THAT(cerr_stream.str(), HasSubstr("<alias> --"));
}

TEST_F(ClientAlias, alias_refuses_creation_unexisting_instance)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}}});

    std::stringstream cout_stream, cerr_stream;
    send_command({"alias", "foo:another_command", "another_alias"}, cout_stream, cerr_stream);

    EXPECT_EQ(cout_stream.str(), "");
    EXPECT_EQ(cerr_stream.str(), "Instance 'foo' does not exist\n");

    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\n");
}

TEST_F(ClientAlias, alias_refuses_creation_rpc_error)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).WillOnce(Return(grpc::Status{grpc::StatusCode::NOT_FOUND, "msg"}));

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}}});

    std::stringstream cout_stream, cerr_stream;
    send_command({"alias", "foo:another_command", "another_alias"}, cout_stream, cerr_stream);

    EXPECT_EQ(cout_stream.str(), "");
    EXPECT_EQ(cerr_stream.str(), "Error retrieving list of instances\n");

    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\n");
}

TEST_F(ClientAlias, unalias_removes_existing_alias)
{
    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}},
                                   {"another_alias", {"another_instance", "another_command"}}});

    EXPECT_EQ(send_command({"unalias", "another_alias"}), mp::ReturnCode::Ok);

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\n");
}

TEST_F(ClientAlias, unalias_succeeds_even_if_script_cannot_be_removed)
{
    EXPECT_CALL(*mock_platform, remove_alias_script(_)).Times(1).WillRepeatedly(Throw(std::runtime_error("bbb")));

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}},
                                   {"another_alias", {"another_instance", "another_command"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "another_alias"}, trash_stream, cerr_stream), mp::ReturnCode::Ok);
    EXPECT_THAT(cerr_stream.str(), Eq("Warning: 'bbb' when removing alias script for another_alias\n"));

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\n");
}

TEST_F(ClientAlias, unalias_does_not_remove_unexisting_alias)
{
    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command"}},
                                   {"another_alias", {"another_instance", "another_command"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "unexisting_alias"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Alias 'unexisting_alias' does not exist\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_EQ(cout_stream.str(), "Alias,Instance,Command\nan_alias,an_instance,a_command\nanother_alias,"
                                 "another_instance,another_command\n");
}

TEST_F(ClientAlias, too_many_unalias_arguments)
{
    std::stringstream cerr_stream;
    send_command({"unalias", "alias_name", "other_argument"}, trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), "Wrong number of arguments given\n");
}

TEST_F(ClientAlias, fails_when_remove_backup_alias_file_fails)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(_)).WillOnce(Return(false)).WillOnce(Return(true)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true)); // mpu::create_temp_file_with_path()
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, remove(_)).WillOnce(Return(false));

    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "alias_name"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), HasSubstr("cannot remove old aliases backup file "));
}

TEST_F(ClientAlias, fails_renaming_alias_file_fails)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(_)).WillOnce(Return(false)).WillOnce(Return(true)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true)); // mpu::create_temp_file_with_path()
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, rename(_, _)).WillOnce(Return(false));

    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "alias_name"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), HasSubstr("cannot rename aliases config to "));
}

TEST_F(ClientAlias, fails_creating_alias_file_fails)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(_)).WillOnce(Return(false)).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true)); // mpu::create_temp_file_with_path()
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, rename(_, _)).WillOnce(Return(false));

    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "alias_name"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), HasSubstr("cannot create aliases config file "));
}

TEST_F(ClientAlias, creating_first_alias_displays_message)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).WillOnce(info_function);

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"alias", "primary:a_command", "an_alias"}, cout_stream), mp::ReturnCode::Ok);

    EXPECT_THAT(cout_stream.str(), HasSubstr("You'll need to add "));
}

TEST_F(ClientAlias, creating_first_alias_does_not_display_message_if_path_is_set)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).WillOnce(info_function);

    auto path = qgetenv("PATH");
#ifdef MULTIPASS_PLATFORM_WINDOWS
    path += ';';
#else
    path += ':';
#endif
    path += MP_PLATFORM.get_alias_scripts_folder().path().toUtf8();
    const auto env_scope = mpt::SetEnvScope{"PATH", path};

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"alias", "primary:a_command", "an_alias"}, cout_stream), mp::ReturnCode::Ok);

    EXPECT_THAT(cout_stream.str(), Eq(""));
}

TEST_F(ClientAlias, fails_when_name_clashes_with_command_alias)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "ls"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), Eq("Alias name 'ls' clashes with a command name\n"));
}

TEST_F(ClientAlias, fails_when_name_clashes_with_command_name)
{
    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "list"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), Eq("Alias name 'list' clashes with a command name\n"));
}
TEST_P(AliasHelpTestSuite, answers_correctly)
{
    auto [command, expected_text] = GetParam();

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"help", command}, cout_stream), mp::ReturnCode::Ok);
    EXPECT_THAT(cout_stream.str(), HasSubstr(expected_text));

    cout_stream.str(std::string());
    EXPECT_EQ(send_command({command, "-h"}, cout_stream), mp::ReturnCode::Ok);
    EXPECT_THAT(cout_stream.str(), HasSubstr(expected_text));
}

INSTANTIATE_TEST_SUITE_P(ClientAlias, AliasHelpTestSuite,
                         Values(std::make_pair(std::string{"alias"},
                                               "Create an alias to be executed on a given instance.\n"),
                                std::make_pair(std::string{"aliases"}, "List available aliases\n"),
                                std::make_pair(std::string{"unalias"}, "Remove an alias\n")));

TEST_P(ClientAliasNameSuite, creates_correct_default_alias_name)
{
    const auto& [command, path] = GetParam();

    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::vector<std::string> arguments{"alias"};
    arguments.push_back(fmt::format("primary:{}{}", path, command));

    EXPECT_EQ(send_command(arguments), mp::ReturnCode::Ok);

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), fmt::format("Alias,Instance,Command\n"
                                               "{},primary,{}{}\n",
                                               command, path, command));
}

INSTANTIATE_TEST_SUITE_P(ClientAlias, ClientAliasNameSuite,
                         Combine(Values("command", "com.mand", "com.ma.nd"),
                                 Values("", "/", "./", "./relative/", "/absolute/", "../more/relative/")));

TEST_P(AliasArgumentCheckTestSuite, answers_correctly)
{
    auto [arguments, expected_return_code, expected_cout, expected_cerr] = GetParam();

    EXPECT_CALL(mock_daemon, info(_, _, _)).Times(AtMost(1)).WillRepeatedly(info_function);

    std::stringstream cout_stream, cerr_stream;
    EXPECT_EQ(send_command(arguments, cout_stream, cerr_stream), expected_return_code);

    EXPECT_THAT(cout_stream.str(), HasSubstr(expected_cout));
    EXPECT_EQ(cerr_stream.str(), expected_cerr);
}

INSTANTIATE_TEST_SUITE_P(
    ClientAlias, AliasArgumentCheckTestSuite,
    Values(std::make_tuple(std::vector<std::string>{"alias"}, mp::ReturnCode::CommandLineError, "",
                           "Wrong number of arguments given\n"),
           std::make_tuple(std::vector<std::string>{"alias", "instance", "command", "alias_name"},
                           mp::ReturnCode::CommandLineError, "", "Wrong number of arguments given\n"),
           std::make_tuple(std::vector<std::string>{"alias", "instance", "alias_name"},
                           mp::ReturnCode::CommandLineError, "", "No command given\n"),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command", "alias_name"}, mp::ReturnCode::Ok,
                           "You'll need to add", ""),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command"}, mp::ReturnCode::Ok,
                           "You'll need to add", ""),
           std::make_tuple(std::vector<std::string>{"alias", ":command"}, mp::ReturnCode::CommandLineError, "",
                           "No instance name given\n"),
           std::make_tuple(std::vector<std::string>{"alias", ":command", "alias_name"},
                           mp::ReturnCode::CommandLineError, "", "No instance name given\n"),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command", "relative/alias_name"},
                           mp::ReturnCode::CommandLineError, "", "Alias has to be a valid filename\n"),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command", "/absolute/alias_name"},
                           mp::ReturnCode::CommandLineError, "", "Alias has to be a valid filename\n"),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command", "weird alias_name"}, mp::ReturnCode::Ok,
                           "You'll need to add", ""),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command", "com.mand"}, mp::ReturnCode::Ok,
                           "You'll need to add", ""),
           std::make_tuple(std::vector<std::string>{"alias", "primary:command", "com.ma.nd"}, mp::ReturnCode::Ok,
                           "You'll need to add", "")));
