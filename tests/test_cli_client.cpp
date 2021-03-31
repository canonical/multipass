/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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
#include "disabling_macros.h"
#include "extra_assertions.h"
#include "fake_alias_config.h"
#include "mock_environment_helpers.h"
#include "mock_settings.h"
#include "mock_standard_paths.h"
#include "mock_stdcin.h"
#include "mock_utils.h"
#include "path.h"
#include "stub_cert_store.h"
#include "stub_certprovider.h"
#include "stub_terminal.h"

#include <src/client/cli/client.h>
#include <src/daemon/daemon_rpc.h>

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>

#include <QKeySequence>
#include <QStringList>
#include <QTemporaryFile>
#include <QtCore/QTemporaryDir>

#include <chrono>
#include <initializer_list>
#include <thread>
#include <utility>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
auto petenv_name()
{
    return MP_SETTINGS.get(mp::petenv_key).toStdString();
}

struct MockDaemonRpc : public mp::DaemonRpc
{
    using mp::DaemonRpc::DaemonRpc; // ctor

    MOCK_METHOD3(create, grpc::Status(grpc::ServerContext* context, const mp::CreateRequest* request,
                                      grpc::ServerWriter<mp::CreateReply>* reply)); // here only to ensure not called
    MOCK_METHOD3(launch, grpc::Status(grpc::ServerContext* context, const mp::LaunchRequest* request,
                                      grpc::ServerWriter<mp::LaunchReply>* reply));
    MOCK_METHOD3(purge, grpc::Status(grpc::ServerContext* context, const mp::PurgeRequest* request,
                                     grpc::ServerWriter<mp::PurgeReply>* response));
    MOCK_METHOD3(find, grpc::Status(grpc::ServerContext* context, const mp::FindRequest* request,
                                    grpc::ServerWriter<mp::FindReply>* response));
    MOCK_METHOD3(info, grpc::Status(grpc::ServerContext* context, const mp::InfoRequest* request,
                                    grpc::ServerWriter<mp::InfoReply>* response));
    MOCK_METHOD3(list, grpc::Status(grpc::ServerContext* context, const mp::ListRequest* request,
                                    grpc::ServerWriter<mp::ListReply>* response));
    MOCK_METHOD3(mount, grpc::Status(grpc::ServerContext* context, const mp::MountRequest* request,
                                     grpc::ServerWriter<mp::MountReply>* response));
    MOCK_METHOD3(recover, grpc::Status(grpc::ServerContext* context, const mp::RecoverRequest* request,
                                       grpc::ServerWriter<mp::RecoverReply>* response));
    MOCK_METHOD3(ssh_info, grpc::Status(grpc::ServerContext* context, const mp::SSHInfoRequest* request,
                                        grpc::ServerWriter<mp::SSHInfoReply>* response));
    MOCK_METHOD3(start, grpc::Status(grpc::ServerContext* context, const mp::StartRequest* request,
                                     grpc::ServerWriter<mp::StartReply>* response));
    MOCK_METHOD3(stop, grpc::Status(grpc::ServerContext* context, const mp::StopRequest* request,
                                    grpc::ServerWriter<mp::StopReply>* response));
    MOCK_METHOD3(suspend, grpc::Status(grpc::ServerContext* context, const mp::SuspendRequest* request,
                                       grpc::ServerWriter<mp::SuspendReply>* response));
    MOCK_METHOD3(restart, grpc::Status(grpc::ServerContext* context, const mp::RestartRequest* request,
                                       grpc::ServerWriter<mp::RestartReply>* response));
    MOCK_METHOD3(delet, grpc::Status(grpc::ServerContext* context, const mp::DeleteRequest* request,
                                     grpc::ServerWriter<mp::DeleteReply>* response));
    MOCK_METHOD3(umount, grpc::Status(grpc::ServerContext* context, const mp::UmountRequest* request,
                                      grpc::ServerWriter<mp::UmountReply>* response));
    MOCK_METHOD3(version, grpc::Status(grpc::ServerContext* context, const mp::VersionRequest* request,
                                       grpc::ServerWriter<mp::VersionReply>* response));
    MOCK_METHOD3(ping,
                 grpc::Status(grpc::ServerContext* context, const mp::PingRequest* request, mp::PingReply* response));
    MOCK_METHOD3(get, grpc::Status(grpc::ServerContext* context, const mp::GetRequest* request,
                                   grpc::ServerWriter<mp::GetReply>* response));
};

struct Client : public Test
{
    auto make_get_reply(const std::string& value)
    {
        return [value](Unused, Unused, grpc::ServerWriter<mp::GetReply>* response) {
            mp::GetReply get_reply;

            get_reply.set_value(value);

            response->Write(get_reply);

            return grpc::Status{};
        };
    }

    void SetUp() override
    {
        EXPECT_CALL(mock_settings, get(_)).Times(AnyNumber()); /* Admit get calls beyond explicitly expected in tests.
                                                                  This allows general actions to consult settings
                                                                  (e.g. Windows Terminal profile sync) */
        EXPECT_CALL(mock_daemon, get(_, Property(&mp::GetRequest::key, StrEq(mp::mounts_key)), _))
            .WillRepeatedly(Invoke(make_get_reply("true"))); // Tests assume this default, but platforms may override.
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), locate(_, _, _))
            .Times(AnyNumber()); // needed to allow general calls once we have added the specific expectation below
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                    locate(_, mpt::match_qstring(EndsWith("settings.json")), _))
            .Times(AnyNumber())
            .WillRepeatedly(Return("")); /* Avoid writing to Windows Terminal settings. We use an "expectation" so that
                                            it gets reset at the end of each test (by VerifyAndClearExpectations) */
    }

    void TearDown() override
    {
        Mock::VerifyAndClearExpectations(&mock_daemon); /* We got away without this before because, being a strict mock
                                                           every call to mock_daemon had to be explicitly "expected".
                                                           Being the best match for incoming calls, each expectation
                                                           took precedence over the previous ones, preventing them from
                                                           being saturated inadvertently */
    }

    int send_command(const std::vector<std::string>& command, std::ostream& cout = trash_stream,
                     std::ostream& cerr = trash_stream, std::istream& cin = trash_stream)
    {
        mpt::StubTerminal term(cout, cerr, cin);
        mp::ClientConfig client_config{server_address, mp::RpcConnectionType::insecure,
                                       std::make_unique<mpt::StubCertProvider>(), &term};
        mp::Client client{client_config};
        QStringList args = QStringList() << "multipass_test";

        for (const auto& arg : command)
        {
            args << QString::fromStdString(arg);
        }
        return client.run(args);
    }

    template <typename Str1, typename Str2>
    std::string keyval_arg(Str1&& key, Str2&& val)
    {
        return fmt::format("{}={}", std::forward<Str1>(key), std::forward<Str2>(val));
    }

    std::string get_setting(const std::initializer_list<std::string>& args)
    {
        auto out = std::ostringstream{};
        std::vector<std::string> cmd{"get"};
        cmd.insert(end(cmd), cbegin(args), cend(args));

        EXPECT_THAT(send_command(cmd, out), Eq(mp::ReturnCode::Ok));

        auto ret = out.str();
        if (!ret.empty())
        {
            EXPECT_EQ(ret.back(), '\n');
            ret.pop_back(); // drop newline
        }

        return ret;
    }

    std::string get_setting(const std::string& key)
    {
        return get_setting({key});
    }

    auto make_automount_matcher(const QTemporaryDir& fake_home) const
    {
        const auto automount_source_matcher =
            Property(&mp::MountRequest::source_path, StrEq(fake_home.path().toStdString()));

        const auto target_instance_matcher = Property(&mp::TargetPathInfo::instance_name, StrEq(petenv_name()));
        const auto target_path_matcher = Property(&mp::TargetPathInfo::target_path, StrEq(mp::home_automount_dir));
        const auto target_info_matcher = AllOf(target_instance_matcher, target_path_matcher);
        const auto automount_target_matcher =
            Property(&mp::MountRequest::target_paths, AllOf(Contains(target_info_matcher), SizeIs(1)));

        return AllOf(automount_source_matcher, automount_target_matcher);
    }

    auto make_launch_instance_matcher(const std::string& instance_name)
    {
        return Property(&mp::LaunchRequest::instance_name, StrEq(instance_name));
    }

    auto make_ssh_info_instance_matcher(const std::string& instance_name)
    {
        return Property(&mp::SSHInfoRequest::instance_name, ElementsAre(StrEq(instance_name)));
    }

    template <typename RequestType, typename Matcher>
    auto make_instances_matcher(const Matcher& instances_matcher)
    {
        return Property(&RequestType::instance_names, Property(&mp::InstanceNames::instance_name, instances_matcher));
    }

    template <typename RequestType, typename Sequence>
    auto make_instances_sequence_matcher(const Sequence& seq)
    {
        return make_instances_matcher<RequestType>(ElementsAreArray(seq));
    }

    template <typename RequestType, int size>
    auto make_instance_in_repeated_field_matcher(const std::string& instance_name)
    {
        static_assert(size > 0, "size must be positive");
        return make_instances_matcher<RequestType>(AllOf(Contains(StrEq(instance_name)), SizeIs(size)));
    }

    template <typename RequestType>
    auto make_request_verbosity_matcher(decltype(std::declval<RequestType>().verbosity_level()) verbosity)
    {
        return Property(&RequestType::verbosity_level, Eq(verbosity));
    }

    template <typename RequestType>
    auto make_request_timeout_matcher(decltype(std::declval<RequestType>().timeout()) timeout)
    {
        return Property(&RequestType::timeout, Eq(timeout));
    }

    void aux_set_cmd_rejects_bad_val(const char* key, const char* val)
    {
        const auto default_val = get_setting(key);
        EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
            .WillRepeatedly(Throw(mp::InvalidSettingsException{key, val, "bad"}));
        EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::CommandLineError));
        EXPECT_THAT(get_setting(key), Eq(default_val));
    }

    auto make_fill_listreply(std::vector<mp::InstanceStatus_Status> statuses)
    {
        return [statuses](Unused, Unused, grpc::ServerWriter<mp::ListReply>* response) {
            mp::ListReply list_reply;

            for (mp::InstanceStatus_Status status : statuses)
            {
                auto list_entry = list_reply.add_instances();
                list_entry->mutable_instance_status()->set_status(status);
            }

            response->Write(list_reply);

            return grpc::Status{};
        };
    }

    std::string negate_flag_string(const std::string& orig)
    {
        auto flag = QVariant{QString::fromStdString(orig)}.toBool();
        return QVariant::fromValue(!flag).toString().toStdString();
    }

#ifdef WIN32
    std::string server_address{"localhost:50051"};
#else
    std::string server_address{"unix:/tmp/test-multipassd.socket"};
#endif
    mpt::StubCertProvider cert_provider;
    mpt::StubCertStore cert_store;
    StrictMock<MockDaemonRpc> mock_daemon{server_address, mp::RpcConnectionType::insecure, cert_provider,
                                          cert_store}; // strict to fail on unexpected calls and play well with sharing
    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance(); /* although this is shared, expectations are
                                                                              reset at the end of each test */
    static std::stringstream trash_stream; // this may have contents (that we don't care about)
};

struct ClientAlias : public Client, public FakeAliasConfig
{
    ClientAlias()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(fake_alias_dir.path()));
    }
};

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

std::stringstream Client::trash_stream; // replace with inline in C++17

// Tests for no postional args given
TEST_F(Client, no_command_is_error)
{
    EXPECT_THAT(send_command({}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, no_command_help_ok)
{
    EXPECT_THAT(send_command({"-h"}), Eq(mp::ReturnCode::Ok));
}

// transfer cli tests
TEST_F(Client, transfer_cmd_good_source_remote)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"transfer", "test-vm:foo", mpt::test_data_path().toStdString() + "good_index.json"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, transfer_cmd_good_destination_remote)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"transfer", mpt::test_data_path().toStdString() + "good_index.json", "test-vm:bar"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, transfer_cmd_help_ok)
{
    EXPECT_THAT(send_command({"transfer", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, transfer_cmd_fails_invalid_source_file)
{
    EXPECT_THAT(send_command({"transfer", "foo", "test-vm:bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_fails_source_is_dir)
{
    EXPECT_THAT(send_command({"transfer", mpt::test_data_path().toStdString(), "test-vm:bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_fails_no_instance)
{
    EXPECT_THAT(send_command({"transfer", mpt::test_data_path().toStdString() + "good_index.json", "."}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_fails_instance_both_source_destination)
{
    EXPECT_THAT(send_command({"transfer", "test-vm1:foo", "test-vm2:bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_fails_too_few_args)
{
    EXPECT_THAT(send_command({"transfer", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_fails_source_path_empty)
{
    EXPECT_THAT(send_command({"transfer", "test-vm1:", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_fails_multiple_sources_destination_file)
{
    EXPECT_THAT(send_command({"transfer", "test-vm1:foo", "test-vm2:bar",
                              mpt::test_data_path().toStdString() + "good_index.json"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_stdin_good_destination_ok)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"transfer", "-", "test-vm1:foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, transfer_cmd_stdout_good_source_ok)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"transfer", "test-vm1:foo", "-"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, transfer_cmd_stdout_stdin_only_fails)
{
    EXPECT_THAT(send_command({"transfer", "-", "-"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transfer_cmd_stdout_stdin_declaration_fails)
{
    EXPECT_THAT(
        send_command({"transfer", "test-vm1:foo", "-", "-", mpt::test_data_path().toStdString() + "good_index.json"}),
        Eq(mp::ReturnCode::CommandLineError));
}

// shell cli test
TEST_F(Client, shell_cmd_good_arguments)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"shell", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_help_ok)
{
    EXPECT_THAT(send_command({"shell", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_no_args_targets_petenv)
{
    const auto petenv_matcher = make_ssh_info_instance_matcher(petenv_name());
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_considers_configured_petenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_ssh_info_instance_matcher(custom_petenv);
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_can_target_petenv_explicitly)
{
    const auto petenv_matcher = make_ssh_info_instance_matcher(petenv_name());
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_launches_petenv_if_absent)
{
    const auto petenv_ssh_info_matcher = make_ssh_info_instance_matcher(petenv_name());
    const auto petenv_launch_matcher = Property(&mp::LaunchRequest::instance_name, StrEq(petenv_name()));
    const grpc::Status ok{}, notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, mount).WillRepeatedly(Return(ok)); // 0 or more times

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_ssh_info_matcher, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, petenv_launch_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_ssh_info_matcher, _)).WillOnce(Return(ok));

    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_automounts_when_launching_petenv)
{
    const grpc::Status ok{}, notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdSkipsAutomountWhenDisabled)
{
    std::stringstream cout_stream;
    const grpc::Status ok{}, notfound{grpc::StatusCode::NOT_FOUND, "msg"};
    EXPECT_CALL(mock_daemon, get(_, Property(&mp::GetRequest::key, StrEq(mp::mounts_key)), _))
        .WillOnce(Invoke(make_get_reply("false")));

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).Times(0);
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", petenv_name()}, cout_stream), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping 'Home' mount due to disabled mounts feature\n"));
}

TEST_F(Client, shell_cmd_forwards_verbosity_to_subcommands)
{
    const grpc::Status ok{}, notfound{grpc::StatusCode::NOT_FOUND, "msg"};
    const auto verbosity = 3;

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, make_request_verbosity_matcher<mp::SSHInfoRequest>(verbosity), _))
        .WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, make_request_verbosity_matcher<mp::LaunchRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, make_request_verbosity_matcher<mp::MountRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, make_request_verbosity_matcher<mp::SSHInfoRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", "-vvv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_forwards_timeout_to_subcommands)
{
    const grpc::Status ok{}, notfound{grpc::StatusCode::NOT_FOUND, "msg"};
    const auto timeout = 123;

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, make_request_timeout_matcher<mp::LaunchRequest>(timeout), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", "--timeout", std::to_string(timeout)}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const grpc::Status ok{}, notfound{grpc::StatusCode::NOT_FOUND, "msg"}, error{grpc::StatusCode::INTERNAL, "oops"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, get).WillOnce(Return(error));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shell_cmd_fails_when_automounting_in_petenv_fails)
{
    const auto ok = grpc::Status{};
    const auto notfound = grpc::Status{grpc::StatusCode::NOT_FOUND, "msg"};
    const auto mount_failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shell_cmd_starts_instance_if_stopped_or_suspended)
{
    const auto instance = "ordinary";
    const auto ssh_info_matcher = make_ssh_info_instance_matcher(instance);
    const auto start_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(instance);
    const grpc::Status ok{}, aborted{grpc::StatusCode::ABORTED, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, ssh_info_matcher, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, start(_, start_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, ssh_info_matcher, _)).WillOnce(Return(ok));

    EXPECT_THAT(send_command({"shell", instance}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_starts_petenv_if_stopped_or_suspended)
{
    const auto ssh_info_matcher = make_ssh_info_instance_matcher(petenv_name());
    const auto start_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name());
    const grpc::Status ok{}, aborted{grpc::StatusCode::ABORTED, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, ssh_info_matcher, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, start(_, start_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, ssh_info_matcher, _)).WillOnce(Return(ok));

    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_fails_if_petenv_present_but_deleted)
{
    const auto petenv_matcher = make_ssh_info_instance_matcher(petenv_name());
    const grpc::Status failed_precond{grpc::StatusCode::FAILED_PRECONDITION, "msg"};

    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _)).WillOnce(Return(failed_precond));
    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shell_cmd_fails_on_other_absent_instance)
{
    const auto instance = "ordinary";
    const auto instance_matcher = make_ssh_info_instance_matcher(instance);
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, ssh_info(_, instance_matcher, _)).WillOnce(Return(notfound));
    EXPECT_THAT(send_command({"shell", instance}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shell_cmd_fails_multiple_args)
{
    EXPECT_THAT(send_command({"shell", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, shell_cmd_fails_unknown_options)
{
    EXPECT_THAT(send_command({"shell", "--not", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, shell_cmd_disabled_petenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).Times(0);
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::CommandLineError));

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).Times(2);
    EXPECT_THAT(send_command({"shell", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"shell", "primary"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shell_cmd_disabled_petenv_help)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_CALL(mock_daemon, ssh_info(_, _, _)).Times(0);
    EXPECT_THAT(send_command({"shell", "-h"}), Eq(mp::ReturnCode::Ok));
}

// launch cli tests
TEST_F(Client, launch_cmd_good_arguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_help_ok)
{
    EXPECT_THAT(send_command({"launch", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_fails_multiple_args)
{
    EXPECT_THAT(send_command({"launch", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_fails_unknown_option)
{
    EXPECT_THAT(send_command({"launch", "-z", "2"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_name_option_ok)
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "-n", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_name_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-n"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_memory_option_ok)
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "-m", "1G"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_memory_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-m"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_ok)
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "-c", "2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_cpu_option_alpha_numeric_fail)
{
    EXPECT_THAT(send_command({"launch", "-c", "w00t"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_alpha_fail)
{
    EXPECT_THAT(send_command({"launch", "-c", "many"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_decimal_fail)
{
    EXPECT_THAT(send_command({"launch", "-c", "1.608"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_zero_fail)
{
    EXPECT_THAT(send_command({"launch", "-c", "0"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_negative_fail)
{
    EXPECT_THAT(send_command({"launch", "-c", "-2"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-c"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, DISABLE_ON_MACOS(launch_cmd_custom_image_file_ok))
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "file://foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, DISABLE_ON_MACOS(launch_cmd_custom_image_http_ok))
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "http://foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_cloudinit_option_with_valid_file_is_ok)
{
    QTemporaryFile tmpfile; // file is auto-deleted when this goes out of scope
    tmpfile.open();
    tmpfile.write("password: passw0rd"); // need some YAML
    tmpfile.close();
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "--cloud-init", qPrintable(tmpfile.fileName())}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_cloudinit_option_fails_with_missing_file)
{
    std::stringstream cerr_stream;
    auto missing_file{"/definitely/missing-file"};

    EXPECT_THAT(send_command({"launch", "--cloud-init", missing_file}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_NE(std::string::npos, cerr_stream.str().find("No such file")) << "cerr has: " << cerr_stream.str();
    EXPECT_NE(std::string::npos, cerr_stream.str().find(missing_file)) << "cerr has: " << cerr_stream.str();
}

TEST_F(Client, launch_cmd_cloudinit_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "--cloud-init"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cloudinit_option_reads_stdin_ok)
{
    MockStdCin cin("password: passw0rd"); // no effect since terminal encapsulation of streams

    std::stringstream ss;
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "--cloud-init", "-"}, trash_stream, trash_stream, ss), Eq(mp::ReturnCode::Ok));
}

#ifndef WIN32 // TODO make home mocking work for windows
TEST_F(Client, launch_cmd_automounts_home_in_petenv)
{
    const auto fake_home = QTemporaryDir{}; // the client checks the mount source exists
    const auto env_scope = mpt::SetEnvScope{"HOME", fake_home.path().toUtf8()};
    const auto home_automount_matcher = make_automount_matcher(fake_home);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name());
    const auto ok = grpc::Status{};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch(_, petenv_launch_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, home_automount_matcher, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name()}), Eq(mp::ReturnCode::Ok));
}
#endif

TEST_F(Client, launchCmdSkipsAutomountWhenDisabled)
{
    const grpc::Status ok{};
    std::stringstream cout_stream;
    EXPECT_CALL(mock_daemon, get(_, Property(&mp::GetRequest::key, StrEq(mp::mounts_key)), _))
        .WillOnce(Invoke(make_get_reply("false")));

    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount).Times(0);

    EXPECT_THAT(send_command({"launch", "--name", petenv_name()}, cout_stream), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping 'Home' mount due to disabled mounts feature\n"));
}

TEST_F(Client, launchCmdOnlyWarnsMountForPetEnv)
{
    const auto invalid_argument = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
    std::stringstream cout_stream;
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("false"));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(invalid_argument));

    EXPECT_THAT(send_command({"launch", "--name", ".asdf"}, cout_stream), Eq(mp::ReturnCode::CommandFail));
    EXPECT_THAT(cout_stream.str(), Not(HasSubstr("Skipping 'Home' mount due to disabled mounts feature\n")));
}

TEST_F(Client, launchCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const auto ok = grpc::Status{};
    const auto error = grpc::Status{grpc::StatusCode::INTERNAL, "oops"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, get).WillOnce(Return(error));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"launch", "--name", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, launch_cmd_fails_when_automounting_in_petenv_fails)
{
    const grpc::Status ok{}, mount_failure{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, launch_cmd_forwards_verbosity_to_subcommands)
{
    const auto ok = grpc::Status{};
    const auto verbosity = 4;

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch(_, make_request_verbosity_matcher<mp::LaunchRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, make_request_verbosity_matcher<mp::MountRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name(), "-vvvv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_does_not_automount_in_normal_instances)
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).Times(0); // because we may want to move from a Strict mock in the future
    EXPECT_THAT(send_command({"launch"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_disabled_petenv_passes)
{
    const auto custom_petenv = "";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_launch_instance_matcher("foo");
    EXPECT_CALL(mock_daemon, launch(_, petenv_matcher, _));

    EXPECT_THAT(send_command({"launch", "--name", "foo"}), Eq(mp::ReturnCode::Ok));
}

struct TestInvalidNetworkOptions : Client, WithParamInterface<std::vector<std::string>>
{
};

TEST_P(TestInvalidNetworkOptions, launch_cmd_return)
{
    auto commands = GetParam();
    commands.insert(commands.begin(), std::string{"launch"});

    EXPECT_CALL(mock_daemon, launch(_, _, _)).Times(0);

    EXPECT_THAT(send_command(commands), Eq(mp::ReturnCode::CommandLineError));
}

INSTANTIATE_TEST_SUITE_P(Client, TestInvalidNetworkOptions,
                         Values(std::vector<std::string>{"--network", "invalid=option"},
                                std::vector<std::string>{"--network"},
                                std::vector<std::string>{"--network", "mode=manual"},
                                std::vector<std::string>{"--network", "mode=manual=auto"},
                                std::vector<std::string>{"--network", "name=eth0,mode=man"},
                                std::vector<std::string>{"--network", "name=eth1,mac=0a"},
                                std::vector<std::string>{"--network", "eth2", "--network"}));

struct TestValidNetworkOptions : Client, WithParamInterface<std::vector<std::string>>
{
};

TEST_P(TestValidNetworkOptions, launch_cmd_return)
{
    auto commands = GetParam();
    commands.insert(commands.begin(), std::string{"launch"});

    EXPECT_CALL(mock_daemon, launch(_, _, _));

    EXPECT_THAT(send_command(commands), Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(Client, TestValidNetworkOptions,
                         Values(std::vector<std::string>{"--network", "eth3"},
                                std::vector<std::string>{"--network", "name=eth4", "--network", "eth5"},
                                std::vector<std::string>{"--network", "name=eth6,mac=01:23:45:67:89:ab"},
                                std::vector<std::string>{"--network", "name=eth7,mode=manual"},
                                std::vector<std::string>{"--network", "name=eth8,mode=auto"},
                                std::vector<std::string>{"--network", "name=eth9", "--network", "name=eth9"},
                                std::vector<std::string>{"--network", "bridged"},
                                std::vector<std::string>{"--network", "name=bridged"},
                                std::vector<std::string>{"--bridged"}));

// purge cli tests
TEST_F(Client, purge_cmd_ok_no_args)
{
    EXPECT_CALL(mock_daemon, purge(_, _, _));
    EXPECT_THAT(send_command({"purge"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, purge_cmd_fails_with_args)
{
    EXPECT_THAT(send_command({"purge", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, purge_cmd_help_ok)
{
    EXPECT_THAT(send_command({"purge", "-h"}), Eq(mp::ReturnCode::Ok));
}

// exec cli tests
TEST_F(Client, exec_cmd_double_dash_ok_cmd_arg)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"exec", "foo", "--", "cmd"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, exec_cmd_double_dash_ok_cmd_arg_with_opts)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"exec", "foo", "--", "cmd", "--foo", "--bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, exec_cmd_double_dash_fails_missing_cmd_arg)
{
    EXPECT_THAT(send_command({"exec", "foo", "--"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, exec_cmd_no_double_dash_ok_cmd_arg)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"exec", "foo", "cmd"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, exec_cmd_no_double_dash_ok_multiple_args)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, exec_cmd_no_double_dash_fails_cmd_arg_with_opts)
{
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "--foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, exec_cmd_help_ok)
{
    EXPECT_THAT(send_command({"exec", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, exec_cmd_no_double_dash_unknown_option_fails_print_suggested_command)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "--unknownOption"}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(
        cerr_stream.str(),
        HasSubstr("Options to the inner command should come after \"--\", like this:\nmultipass exec <instance> -- "
                  "<command> <arguments>\n"));
}

TEST_F(Client, exec_cmd_double_dash_unknown_option_fails_does_not_print_suggested_command)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(send_command({"exec", "foo", "--unknownOption", "--", "cmd"}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(
        cerr_stream.str(),
        Not(HasSubstr("Options to the inner command should come after \"--\", like this:\nmultipass exec <instance> -- "
                      "<command> <arguments>\n")));
}

TEST_F(Client, exec_cmd_no_double_dash_no_unknown_option_fails_does_not_print_suggested_command)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "--help"}, trash_stream, cerr_stream), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(
        cerr_stream.str(),
        Not(HasSubstr("Options to the inner command should come after \"--\", like this:\nmultipass exec <instance> -- "
                      "<command> <arguments>\n")));
}

// help cli tests
TEST_F(Client, help_cmd_ok_with_valid_single_arg)
{
    EXPECT_THAT(send_command({"help", "launch"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, help_cmd_ok_no_args)
{
    EXPECT_THAT(send_command({"help"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, help_cmd_fails_with_invalid_arg)
{
    EXPECT_THAT(send_command({"help", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, help_cmd_help_ok)
{
    EXPECT_THAT(send_command({"help", "-h"}), Eq(mp::ReturnCode::Ok));
}

// info cli tests
TEST_F(Client, info_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"info"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, info_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, info(_, _, _));
    EXPECT_THAT(send_command({"info", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, info(_, _, _));
    EXPECT_THAT(send_command({"info", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_help_ok)
{
    EXPECT_THAT(send_command({"info", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, info(_, _, _));
    EXPECT_THAT(send_command({"info", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"info", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

// list cli tests
TEST_F(Client, list_cmd_ok_no_args)
{
    EXPECT_CALL(mock_daemon, list(_, Property(&mp::ListRequest::request_ipv4, IsTrue()), _));
    EXPECT_THAT(send_command({"list"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, list_cmd_fails_with_args)
{
    EXPECT_THAT(send_command({"list", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, list_cmd_help_ok)
{
    EXPECT_THAT(send_command({"list", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, list_cmd_no_ipv4_ok)
{
    EXPECT_CALL(mock_daemon, list(_, Property(&mp::ListRequest::request_ipv4, IsFalse()), _));
    EXPECT_THAT(send_command({"list", "--no-ipv4"}), Eq(mp::ReturnCode::Ok));
}

// mount cli tests
// Note: mpt::test_data_path() returns an absolute path
TEST_F(Client, mount_cmd_good_absolute_source_path)
{
    EXPECT_CALL(mock_daemon, mount(_, _, _));
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "test-vm:test"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_good_relative_source_path)
{
    EXPECT_CALL(mock_daemon, mount(_, _, _));
    EXPECT_THAT(send_command({"mount", "..", "test-vm:test"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_fails_invalid_source_path)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path_for("foo").toStdString(), "test-vm:test"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mount_cmd_good_valid_uid_map)
{
    EXPECT_CALL(mock_daemon, mount(_, _, _));
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-u", "1000:501", "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_good_valid_large_uid_map)
{
    EXPECT_CALL(mock_daemon, mount(_, _, _));
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-u", "218038053:0", "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_fails_invalid_string_uid_map)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-u", "foo:bar", "test-vm:test"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mount_cmd_fails_invalid_host_int_uid_map)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-u", "5000000000:0", "test-vm:test"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mount_cmd_good_valid_gid_map)
{
    EXPECT_CALL(mock_daemon, mount(_, _, _));
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-g", "1000:501", "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_good_valid_large_gid_map)
{
    EXPECT_CALL(mock_daemon, mount(_, _, _));
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-g", "218038053:0", "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_fails_invalid_string_gid_map)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-g", "foo:bar", "test-vm:test"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mount_cmd_fails_invalid_host_int_gid_map)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-g", "5000000000:0", "test-vm:test"}),
                Eq(mp::ReturnCode::CommandLineError));
}

// recover cli tests
TEST_F(Client, recover_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"recover"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, recover_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, recover(_, _, _));
    EXPECT_THAT(send_command({"recover", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, recover(_, _, _));
    EXPECT_THAT(send_command({"recover", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_help_ok)
{
    EXPECT_THAT(send_command({"recover", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, recover(_, _, _));
    EXPECT_THAT(send_command({"recover", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"recover", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

// start cli tests
TEST_F(Client, start_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, start(_, _, _));
    EXPECT_THAT(send_command({"start", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, start(_, _, _));
    EXPECT_THAT(send_command({"start", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_help_ok)
{
    EXPECT_THAT(send_command({"start", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, start(_, _, _));
    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"start", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, start_cmd_no_args_targets_petenv)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_considers_configured_petenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_can_target_petenv_explicitly)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_can_target_petenv_among_others)
{
    const auto petenv_matcher2 = make_instance_in_repeated_field_matcher<mp::StartRequest, 2>(petenv_name());
    const auto petenv_matcher4 = make_instance_in_repeated_field_matcher<mp::StartRequest, 4>(petenv_name());

    InSequence s;
    EXPECT_CALL(mock_daemon, start(_, _, _));
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher2, _)).Times(2);
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher4, _));
    EXPECT_THAT(send_command({"start", "primary"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", "foo", petenv_name()}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", petenv_name(), "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", "foo", petenv_name(), "bar", "baz"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_disabled_petenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, start(_, _, _));

    EXPECT_THAT(send_command({"start", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, start_cmd_disabled_petenv_all)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, start(_, _, _));

    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_disabled_petenv_help)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, start(_, _, _)).Times(0);

    EXPECT_THAT(send_command({"start", "-h"}), Eq(mp::ReturnCode::Ok));
}

// version cli tests
TEST_F(Client, version_without_arg)
{
    EXPECT_CALL(mock_daemon, version(_, _, _));
    EXPECT_THAT(send_command({"version"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, version_with_positional_format_arg)
{
    EXPECT_THAT(send_command({"version", "format"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, version_with_option_format_arg)
{
    EXPECT_CALL(mock_daemon, version(_, _, _)).Times(4);
    EXPECT_THAT(send_command({"version", "--format=table"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"version", "--format=yaml"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"version", "--format=json"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"version", "--format=csv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, version_with_option_format_invalid_arg)
{
    EXPECT_THAT(send_command({"version", "--format=default"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"version", "--format=MumboJumbo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, version_parse_failure)
{
    EXPECT_THAT(send_command({"version", "--format"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, version_info_on_failure)
{
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, version(_, _, _)).WillOnce(Return(notfound));
    EXPECT_THAT(send_command({"version", "--format=yaml"}), Eq(mp::ReturnCode::Ok));
}

namespace
{
grpc::Status aborted_start_status(const std::vector<std::string>& absent_instances = {},
                                  const std::vector<std::string>& deleted_instances = {})
{
    mp::StartError start_error{};
    auto* errors = start_error.mutable_instance_errors();

    for (const auto& instance : absent_instances)
        errors->insert({instance, mp::StartError::DOES_NOT_EXIST});

    for (const auto& instance : deleted_instances)
        errors->insert({instance, mp::StartError::INSTANCE_DELETED});

    return {grpc::StatusCode::ABORTED, "fakemsg", start_error.SerializeAsString()};
}

std::vector<std::string> concat(const std::vector<std::string>& v1, const std::vector<std::string>& v2)
{
    auto ret = v1;
    ret.insert(end(ret), cbegin(v2), cend(v2));

    return ret;
}
} // namespace

TEST_F(Client, start_cmd_launches_petenv_if_absent)
{
    const auto petenv_start_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name());
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name());
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});

    EXPECT_CALL(mock_daemon, mount).WillRepeatedly(Return(ok)); // 0 or more times

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, petenv_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, petenv_launch_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, petenv_start_matcher, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_automounts_when_launching_petenv)
{
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, _, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, _, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdSkipsAutomountWhenDisabled)
{
    std::stringstream cout_stream;
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});
    EXPECT_CALL(mock_daemon, get(_, Property(&mp::GetRequest::key, StrEq(mp::mounts_key)), _))
        .WillOnce(Invoke(make_get_reply("false")));

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, _, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).Times(0);
    EXPECT_CALL(mock_daemon, start(_, _, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", petenv_name()}, cout_stream), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping 'Home' mount due to disabled mounts feature\n"));
}

TEST_F(Client, start_cmd_forwards_verbosity_to_subcommands)
{
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});
    const auto verbosity = 2;

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, make_request_verbosity_matcher<mp::StartRequest>(verbosity), _))
        .WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, make_request_verbosity_matcher<mp::LaunchRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, make_request_verbosity_matcher<mp::MountRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, make_request_verbosity_matcher<mp::StartRequest>(verbosity), _))
        .WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", "-vv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_forwards_timeout_to_subcommands)
{
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});
    const auto timeout = 123;

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, make_request_timeout_matcher<mp::StartRequest>(timeout), _))
        .WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, make_request_timeout_matcher<mp::LaunchRequest>(timeout), _))
        .WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, make_request_timeout_matcher<mp::StartRequest>(timeout), _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", "--timeout", std::to_string(timeout)}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const auto ok = grpc::Status{};
    const auto aborted = aborted_start_status({petenv_name()});
    const auto error = grpc::Status{grpc::StatusCode::INTERNAL, "oops"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, start).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, get).WillOnce(Return(error));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_fails_when_automounting_in_petenv_fails)
{
    const auto ok = grpc::Status{};
    const auto aborted = aborted_start_status({petenv_name()});
    const auto mount_failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, _, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, _, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_launches_petenv_if_absent_among_others_present)
{
    std::vector<std::string> instances{"a", "b", petenv_name(), "c"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name());
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});

    EXPECT_CALL(mock_daemon, mount).WillRepeatedly(Return(ok)); // 0 or more times

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, petenv_launch_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_fails_if_petenv_if_absent_amont_others_absent)
{
    std::vector<std::string> instances{"a", "b", "c", petenv_name(), "xyz"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({std::next(std::cbegin(instances), 2), std::cend(instances)});

    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_fails_if_petenv_if_absent_amont_others_deleted)
{
    std::vector<std::string> instances{"nope", petenv_name()}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {instances.front()});

    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_fails_if_petenv_present_but_deleted)
{
    const auto petenv_start_matcher = make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name());
    const grpc::Status aborted = aborted_start_status({}, {petenv_name()});

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, petenv_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_fails_if_petenv_present_but_deleted_among_others)
{
    std::vector<std::string> instances{petenv_name(), "other"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {instances.front()});

    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_fails_on_other_absent_instance)
{
    std::vector<std::string> instances{"o-o", "O_o"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {"O_o"});

    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_fails_on_other_absent_instances_with_petenv)
{
    std::vector<std::string> cmd{"start"}, instances{petenv_name(), "lala", "zzz"};
    cmd.insert(end(cmd), cbegin(instances), cend(instances));

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {"zzz"});
    EXPECT_CALL(mock_daemon, start(_, instance_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, start_cmd_does_not_add_petenv_to_others)
{
    const auto matcher = make_instances_matcher<mp::StartRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, start(_, matcher, _));
    EXPECT_THAT(send_command({"start", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_does_not_add_petenv_to_all)
{
    const auto matcher = make_instances_matcher<mp::StartRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, start(_, matcher, _));
    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

// stop cli tests
TEST_F(Client, stop_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_THAT(send_command({"stop", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_THAT(send_command({"stop", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_help_ok)
{
    EXPECT_THAT(send_command({"stop", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"stop", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_no_args_targets_petenv)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_considers_configured_petenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_can_target_petenv_explicitly)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"stop", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_can_target_petenv_among_others)
{
    const auto petenv_matcher2 = make_instance_in_repeated_field_matcher<mp::StopRequest, 2>(petenv_name());
    const auto petenv_matcher4 = make_instance_in_repeated_field_matcher<mp::StopRequest, 4>(petenv_name());

    InSequence s;
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher2, _)).Times(2);
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher4, _));
    EXPECT_THAT(send_command({"stop", "primary"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", "foo", petenv_name()}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", petenv_name(), "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", "foo", petenv_name(), "bar", "baz"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_does_not_add_petenv_to_others)
{
    const auto matcher = make_instances_matcher<mp::StopRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, stop(_, matcher, _));
    EXPECT_THAT(send_command({"stop", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_does_not_add_petenv_to_all)
{
    const auto matcher = make_instances_matcher<mp::StopRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, stop(_, matcher, _));
    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_fails_with_time_and_cancel)
{
    EXPECT_THAT(send_command({"stop", "--time", "+10", "--cancel", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_succeeds_with_plus_time)
{
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+10"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_succeeds_with_no_plus_time)
{
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_THAT(send_command({"stop", "foo", "--time", "10"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_fails_with_invalid_time_prefix)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "-10"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_fails_with_invalid_time)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_fails_with_time_suffix)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+10s"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_succeds_with_cancel)
{
    EXPECT_CALL(mock_daemon, stop(_, _, _));
    EXPECT_THAT(send_command({"stop", "foo", "--cancel"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_no_args_time_option_delays_petenv_shutdown)
{
    const auto delay = 5;
    const auto matcher = AllOf(make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name()),
                               Property(&mp::StopRequest::time_minutes, delay));
    EXPECT_CALL(mock_daemon, stop(_, matcher, _));
    EXPECT_THAT(send_command({"stop", "--time", std::to_string(delay)}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_no_args_cancel_option_cancels_delayed_petenv_shutdown)
{
    const auto matcher = AllOf(make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name()),
                               Property(&mp::StopRequest::cancel_shutdown, true));
    EXPECT_CALL(mock_daemon, stop(_, matcher, _));
    EXPECT_THAT(send_command({"stop", "--cancel"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_no_args_fails_with_time_and_cancel)
{
    EXPECT_THAT(send_command({"stop", "--time", "+10", "--cancel"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_disabled_petenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "--cancel"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "--time", "10"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_disabled_petenv_with_instance)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, stop(_, _, _));

    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", "--cancel"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "--time", "10"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_disabled_petenv_help)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"stop", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_disabled_petenv_all)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, stop(_, _, _));

    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

// suspend cli tests
TEST_F(Client, suspend_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, suspend(_, _, _)).Times(2);
    EXPECT_THAT(send_command({"suspend", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"suspend", "primary"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, suspend(_, _, _));
    EXPECT_THAT(send_command({"suspend", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_help_ok)
{
    EXPECT_THAT(send_command({"suspend", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, suspend(_, _, _));
    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_no_args_targets_petenv)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::SuspendRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, suspend(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_considers_configured_petenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::SuspendRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, suspend(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_can_target_petenv_explicitly)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::SuspendRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, suspend(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"suspend", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_can_target_petenv_among_others)
{
    const auto petenv_matcher2 = make_instance_in_repeated_field_matcher<mp::SuspendRequest, 2>(petenv_name());
    const auto petenv_matcher4 = make_instance_in_repeated_field_matcher<mp::SuspendRequest, 4>(petenv_name());

    InSequence s;
    EXPECT_CALL(mock_daemon, suspend(_, petenv_matcher2, _)).Times(2);
    EXPECT_CALL(mock_daemon, suspend(_, petenv_matcher4, _));
    EXPECT_THAT(send_command({"suspend", "foo", petenv_name()}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"suspend", petenv_name(), "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"suspend", "foo", petenv_name(), "bar", "baz"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_does_not_add_petenv_to_others)
{
    const auto matcher = make_instances_matcher<mp::SuspendRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, suspend(_, matcher, _));
    EXPECT_THAT(send_command({"suspend", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_does_not_add_petenv_to_all)
{
    const auto matcher = make_instances_matcher<mp::SuspendRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, suspend(_, matcher, _));
    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"suspend", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, suspend_cmd_disabled_petenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, suspend(_, _, _));

    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"suspend", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_disabled_petenv_help)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"suspend", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_disabled_petenv_all)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, suspend(_, _, _));

    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

// restart cli tests
TEST_F(Client, restart_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, restart(_, _, _)).Times(2);
    EXPECT_THAT(send_command({"restart", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"restart", "primary"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, restart(_, _, _));
    EXPECT_THAT(send_command({"restart", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_help_ok)
{
    EXPECT_THAT(send_command({"restart", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, restart(_, _, _));
    EXPECT_THAT(send_command({"restart", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_no_args_targets_petenv)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, restart(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_considers_configured_petenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, restart(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_can_target_petenv_explicitly)
{
    const auto petenv_matcher = make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(petenv_name());
    EXPECT_CALL(mock_daemon, restart(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"restart", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_can_target_petenv_among_others)
{
    const auto petenv_matcher2 = make_instance_in_repeated_field_matcher<mp::RestartRequest, 2>(petenv_name());
    const auto petenv_matcher4 = make_instance_in_repeated_field_matcher<mp::RestartRequest, 4>(petenv_name());

    InSequence s;
    EXPECT_CALL(mock_daemon, restart(_, petenv_matcher2, _)).Times(2);
    EXPECT_CALL(mock_daemon, restart(_, petenv_matcher4, _));
    EXPECT_THAT(send_command({"restart", "foo", petenv_name()}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"restart", petenv_name(), "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"restart", "foo", petenv_name(), "bar", "baz"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_does_not_add_petenv_to_others)
{
    const auto matcher = make_instances_matcher<mp::RestartRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, restart(_, matcher, _));
    EXPECT_THAT(send_command({"restart", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_does_not_add_petenv_to_all)
{
    const auto matcher = make_instances_matcher<mp::RestartRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, restart(_, matcher, _));
    EXPECT_THAT(send_command({"restart", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"restart", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, restart_cmd_fails_with_unknown_options)
{
    EXPECT_THAT(send_command({"restart", "-x", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-wrong", "--all"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-h", "--nope", "not"}), Eq(mp::ReturnCode::CommandLineError));

    // Options that would be accepted by stop
    EXPECT_THAT(send_command({"restart", "-t", "foo"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-t0", "bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "--time", "42", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-c", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "--cancel", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, restart_cmd_disabled_petenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, restart(_, _, _));

    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_disabled_petenv_help)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"restart", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_disabled_petenv_all)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, restart(_, _, _));

    EXPECT_THAT(send_command({"restart", "--all"}), Eq(mp::ReturnCode::Ok));
}

// delete cli tests
TEST_F(Client, delete_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"delete"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, delete_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, delet(_, _, _));
    EXPECT_THAT(send_command({"delete", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, delete_cmd_succeeds_with_multiple_args)
{
    EXPECT_CALL(mock_daemon, delet(_, _, _));
    EXPECT_THAT(send_command({"delete", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, delete_cmd_help_ok)
{
    EXPECT_THAT(send_command({"delete", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, delete_cmd_succeeds_with_all)
{
    EXPECT_CALL(mock_daemon, delet(_, _, _));
    EXPECT_THAT(send_command({"delete", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, delete_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"delete", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, delete_cmd_accepts_purge_option)
{
    EXPECT_CALL(mock_daemon, delet(_, _, _)).Times(2);
    EXPECT_THAT(send_command({"delete", "--purge", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"delete", "-p", "bar"}), Eq(mp::ReturnCode::Ok));
}

// find cli tests
TEST_F(Client, find_cmd_unsupported_option_ok)
{
    EXPECT_CALL(mock_daemon, find(_, _, _));
    EXPECT_THAT(send_command({"find", "--show-unsupported"}), Eq(mp::ReturnCode::Ok));
}

// get/set cli tests
struct TestBasicGetSetOptions : Client, WithParamInterface<const char*>
{
};

TEST_P(TestBasicGetSetOptions, get_can_read_settings)
{
    const auto& key = GetParam();
    EXPECT_CALL(mock_settings, get(Eq(key)));
    get_setting(key);
}

TEST_P(TestBasicGetSetOptions, set_can_write_settings)
{
    const auto& key = GetParam();
    const auto val = "blah";

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::Ok));
}

TEST_P(TestBasicGetSetOptions, set_cmd_allows_empty_val)
{
    const auto& key = GetParam();
    const auto val = "";

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(Client, TestBasicGetSetOptions,
                         Values(mp::petenv_key, mp::driver_key, mp::autostart_key, mp::hotkey_key,
                                mp::bridged_interface_key, mp::mounts_key));

TEST_F(Client, get_cmd_fails_with_no_arguments)
{
    EXPECT_THAT(send_command({"get"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, set_cmd_fails_with_no_arguments)
{
    EXPECT_CALL(mock_settings, set(_, _)).Times(0);
    EXPECT_THAT(send_command({"set"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, get_cmd_fails_with_multiple_arguments)
{
    EXPECT_THAT(send_command({"get", mp::petenv_key, mp::driver_key}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, set_cmd_fails_with_multiple_arguments)
{
    EXPECT_CALL(mock_settings, set(_, _)).Times(0);
    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, "asdf"), keyval_arg(mp::driver_key, "qemu")}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, set_cmd_fails_with_bad_key_val_format)
{
    EXPECT_CALL(mock_settings, set(_, _)).Times(0); // this is not where the rejection is here
    EXPECT_THAT(send_command({"set", "="}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "abc"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "=abc"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "foo=bar="}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "=foo=bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "=foo=bar="}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "foo=bar=="}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "==foo=bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "foo==bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "foo===bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"set", "x=x=x"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, get_cmd_fails_with_unknown_key)
{
    const auto key = "wrong.key";
    EXPECT_CALL(mock_settings, get(Eq(key)));
    EXPECT_THAT(send_command({"get", key}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, set_cmd_fails_with_unknown_key)
{
    const auto key = "wrong.key";
    const auto val = "blah";
    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, get_handles_persistent_settings_errors)
{
    const auto key = mp::petenv_key;
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Throw(mp::PersistentSettingsException{"op", "test"}));
    EXPECT_THAT(send_command({"get", key}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, get_returns_special_representation_of_empty_value_by_default)
{
    const auto key = mp::hotkey_key;
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(QStringLiteral("")));
    EXPECT_THAT(get_setting(key), Eq("<empty>"));
}

TEST_F(Client, get_returns_empty_string_on_empty_value_with_raw_option)
{
    const auto key = mp::hotkey_key;
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(QStringLiteral("")));
    EXPECT_THAT(get_setting({key, "--raw"}), IsEmpty());
}

TEST_F(Client, get_keeps_other_values_untouched_with_raw_option)
{
    std::vector<std::pair<const char*, QString>> keyvals{{mp::autostart_key, QStringLiteral("False")},
                                                         {mp::petenv_key, QStringLiteral("a-pet-nAmE")},
                                                         {mp::hotkey_key, QStringLiteral("Ctrl+Alt+U")}};
    for (const auto& [key, val] : keyvals)
    {
        EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(val));
        EXPECT_THAT(get_setting({key, "--raw"}), Eq(val.toStdString()));
    }
}

TEST_F(Client, set_handles_persistent_settings_errors)
{
    const auto key = mp::petenv_key;
    const auto val = "asdasdasd";
    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val))).WillOnce(Throw(mp::PersistentSettingsException{"op", "test"}));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, get_returns_acceptable_autostart_value_by_default)
{
    EXPECT_THAT(get_setting(mp::autostart_key), AnyOf("true", "false"));
}

TEST_F(Client, get_returns_normalized_hotkey_by_default)
{
    auto hotkey = QString::fromStdString(get_setting(mp::hotkey_key));
    EXPECT_EQ(hotkey, (QKeySequence{hotkey, QKeySequence::NativeText}.toString(QKeySequence::NativeText)));
}

TEST_F(Client, set_cmd_rejects_bad_autostart_values)
{
    aux_set_cmd_rejects_bad_val(mp::autostart_key, "asdf");
    aux_set_cmd_rejects_bad_val(mp::autostart_key, "trueasdf");
    aux_set_cmd_rejects_bad_val(mp::autostart_key, "123");
    aux_set_cmd_rejects_bad_val(mp::autostart_key, "");
}

TEST_F(Client, get_and_set_can_read_and_write_autostart_flag)
{
    const auto orig = get_setting((mp::autostart_key));
    const auto novel = negate_flag_string(orig);

    EXPECT_CALL(mock_settings, set(Eq(mp::autostart_key), Eq(QString::fromStdString(novel))));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::autostart_key, novel)}), Eq(mp::ReturnCode::Ok));

    EXPECT_CALL(mock_settings, get(Eq(mp::autostart_key))).WillRepeatedly(Return(QString::fromStdString(novel)));
    EXPECT_THAT(get_setting(mp::autostart_key), Eq(novel));
}

TEST_F(Client, get_and_set_can_read_and_write_primary_name)
{
    const auto name = "xyz";
    const auto petenv_matcher = make_ssh_info_instance_matcher(name);

    EXPECT_THAT(get_setting(mp::petenv_key), AllOf(Not(IsEmpty()), StrNe(name)));

    EXPECT_CALL(mock_settings, set(Eq(mp::petenv_key), Eq(name)));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, name)}), Eq(mp::ReturnCode::Ok));

    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(name));
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, get_returns_acceptable_primary_name_by_default)
{
    const auto default_name = get_setting(mp::petenv_key);
    const auto petenv_matcher = make_ssh_info_instance_matcher(default_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));

    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, default_name)}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(get_setting(mp::petenv_key), Eq(default_name));
}

TEST_F(Client, set_cmd_rejects_bad_primary_name)
{
    const auto key = mp::petenv_key;
    const auto default_petenv_matcher = make_ssh_info_instance_matcher(get_setting(key));

    aux_set_cmd_rejects_bad_val(key, "123.badname_");

    EXPECT_CALL(mock_daemon, ssh_info(_, default_petenv_matcher, _));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, set_cmd_rejects_bad_driver)
{
    aux_set_cmd_rejects_bad_val(mp::driver_key, "bad driver");
    aux_set_cmd_rejects_bad_val(mp::driver_key, "");
}

TEST_F(Client, set_cmd_falls_through_instances_when_no_driver_change)
{
    const auto default_driver = MP_SETTINGS.get(mp::driver_key);
    EXPECT_CALL(mock_daemon, list(_, _, _)).Times(0);
    EXPECT_THAT(send_command({"set", keyval_arg(mp::driver_key, default_driver)}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, set_cmd_falls_through_instances_when_another_driver)
{
    EXPECT_CALL(mock_daemon, list(_, _, _)).Times(0);
    aux_set_cmd_rejects_bad_val(mp::driver_key, "other");
}

#ifdef MULTIPASS_PLATFORM_LINUX // These tests concern linux-specific behavior for qemu<->libvirt switching

TEST_F(Client, set_cmd_fails_driver_switch_when_needs_daemon_and_grpc_problem)
{
    EXPECT_CALL(mock_daemon, list(_, _, _)).WillOnce(Return(grpc::Status{grpc::StatusCode::ABORTED, "msg"}));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::driver_key, "libvirt")}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, set_cmd_succeeds_when_daemon_not_around)
{
    EXPECT_CALL(mock_daemon, list(_, _, _)).WillOnce(Return(grpc::Status{grpc::StatusCode::NOT_FOUND, "msg"}));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::driver_key, "libvirt")}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, set_cmd_toggle_petenv)
{
    EXPECT_CALL(mock_settings, set(Eq(mp::petenv_key), Eq("")));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, "")}), Eq(mp::ReturnCode::Ok));

    EXPECT_CALL(mock_settings, set(Eq(mp::petenv_key), Eq("some primary")));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, "some primary")}), Eq(mp::ReturnCode::Ok));
}

struct TestSetDriverWithInstances
    : Client,
      WithParamInterface<std::pair<std::vector<mp::InstanceStatus_Status>, mp::ReturnCode>>
{
};

const std::vector<std::pair<std::vector<mp::InstanceStatus_Status>, mp::ReturnCode>> set_driver_expected{
    {{}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::STOPPED}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::DELETED}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::STOPPED, mp::InstanceStatus::STOPPED}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::STOPPED, mp::InstanceStatus::DELETED}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::DELETED, mp::InstanceStatus::DELETED}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::DELETED, mp::InstanceStatus::STOPPED}, mp::ReturnCode::Ok},
    {{mp::InstanceStatus::RUNNING}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::STARTING}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::RESTARTING}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::DELAYED_SHUTDOWN}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::SUSPENDING}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::SUSPENDED}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::UNKNOWN}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::RUNNING, mp::InstanceStatus::STOPPED}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::STARTING, mp::InstanceStatus::STOPPED}, mp::ReturnCode::CommandFail},
    {{mp::InstanceStatus::SUSPENDED, mp::InstanceStatus::STOPPED}, mp::ReturnCode::CommandFail},
};

TEST_P(TestSetDriverWithInstances, inspects_instance_states)
{
    EXPECT_CALL(mock_daemon, list(_, _, _)).WillOnce(Invoke(make_fill_listreply(GetParam().first)));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::driver_key, "libvirt")}), Eq(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(Client, TestSetDriverWithInstances, ValuesIn(set_driver_expected));

#endif // MULTIPASS_PLATFORM_LINUX

#ifndef MULTIPASS_PLATFORM_WINDOWS // Test Windows Terminal setting not recognized outside Windows

TEST_F(Client, get_and_set_do_not_know_about_winterm_integration)
{
    const auto val = "asdf";
    EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key)));
    EXPECT_THAT(send_command({"get", mp::winterm_key}), Eq(mp::ReturnCode::CommandLineError));

    EXPECT_CALL(mock_settings, set(Eq(mp::winterm_key), Eq(val)));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::winterm_key, val)}), Eq(mp::ReturnCode::CommandLineError));
}

#else

TEST_F(Client, get_and_set_can_read_and_write_winterm_integration)
{
    const auto orig = get_setting((mp::winterm_key));
    const auto novel = "asdf";

    EXPECT_THAT(get_setting(mp::winterm_key), Not(IsEmpty()));

    EXPECT_CALL(mock_settings, set(Eq(mp::winterm_key), Eq(QString::fromStdString(novel))));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::winterm_key, novel)}), Eq(mp::ReturnCode::Ok));
}

#endif // #ifndef MULTIPASS_PLATFORM_WINDOWS

TEST_F(Client, getReturnsAcceptableMountsValueByDefault)
{
    EXPECT_THAT(get_setting(mp::mounts_key), AnyOf("true", "false"));
}

TEST_F(Client, setCmdRejectsBadMountsValues)
{
    aux_set_cmd_rejects_bad_val(mp::mounts_key, "asdf");
    aux_set_cmd_rejects_bad_val(mp::mounts_key, "trueasdf");
    aux_set_cmd_rejects_bad_val(mp::mounts_key, "123");
    aux_set_cmd_rejects_bad_val(mp::mounts_key, "");
}

TEST_F(Client, getAndSetCanReadAndWriteMountsFlag)
{
    const auto orig = get_setting((mp::mounts_key));
    const auto novel = negate_flag_string(orig);

    EXPECT_CALL(mock_settings, set(Eq(mp::mounts_key), Eq(QString::fromStdString(novel))));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::mounts_key, novel)}), Eq(mp::ReturnCode::Ok));

    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return(QString::fromStdString(novel)));
    EXPECT_THAT(get_setting(mp::mounts_key), Eq(novel));
}

// general help tests
TEST_F(Client, help_returns_ok_return_code)
{
    EXPECT_THAT(send_command({"--help"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, command_help_is_different_than_general_help)
{
    std::stringstream general_help_output;
    send_command({"--help"}, general_help_output);

    std::stringstream command_output;
    send_command({"list", "--help"}, command_output);

    EXPECT_THAT(general_help_output.str(), Ne(command_output.str()));
}

TEST_F(Client, help_cmd_launch_same_launch_cmd_help)
{
    std::stringstream help_cmd_launch;
    send_command({"help", "launch"}, help_cmd_launch);

    std::stringstream launch_cmd_help;
    send_command({"launch", "-h"}, launch_cmd_help);

    EXPECT_THAT(help_cmd_launch.str(), Ne(""));
    EXPECT_THAT(help_cmd_launch.str(), Eq(launch_cmd_help.str()));
}

const std::vector<std::string> timeout_commands{"launch", "start", "restart", "shell"};
const std::vector<std::string> valid_timeouts{"120", "1234567"};
const std::vector<std::string> invalid_timeouts{"-1", "0", "a", "3min", "15.51", ""};

struct TimeoutCorrectSuite : Client, WithParamInterface<std::tuple<std::string, std::string>>
{
};

TEST_P(TimeoutCorrectSuite, cmds_with_timeout_ok)
{
    const auto& [command, timeout] = GetParam();

    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_THAT(send_command({command, "--timeout", timeout}), Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutCorrectSuite, Combine(ValuesIn(timeout_commands), ValuesIn(valid_timeouts)));

struct TimeoutNullSuite : Client, WithParamInterface<std::string>
{
};

TEST_P(TimeoutNullSuite, cmds_with_timeout_null_bad)
{
    EXPECT_THAT(send_command({GetParam(), "--timeout"}), Eq(mp::ReturnCode::CommandLineError));
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutNullSuite, ValuesIn(timeout_commands));

struct TimeoutInvalidSuite : Client, WithParamInterface<std::tuple<std::string, std::string>>
{
};

TEST_P(TimeoutInvalidSuite, cmds_with_invalid_timeout_bad)
{
    std::stringstream cerr_stream;
    const auto& [command, timeout] = GetParam();

    EXPECT_THAT(send_command({command, "--timeout", timeout}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));

    EXPECT_EQ(cerr_stream.str(), "error: --timeout value has to be a positive integer\n");
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutInvalidSuite, Combine(ValuesIn(timeout_commands), ValuesIn(invalid_timeouts)));

struct TimeoutSuite : Client, WithParamInterface<std::string>
{
    void SetUp() override
    {
        Client::SetUp();

        ON_CALL(mock_daemon, launch).WillByDefault(request_sleeper<mp::LaunchRequest, mp::LaunchReply>);
        ON_CALL(mock_daemon, start).WillByDefault(request_sleeper<mp::StartRequest, mp::StartReply>);
        ON_CALL(mock_daemon, restart).WillByDefault(request_sleeper<mp::RestartRequest, mp::RestartReply>);
        ON_CALL(mock_daemon, ssh_info).WillByDefault(request_sleeper<mp::SSHInfoRequest, mp::SSHInfoReply>);
    }

    template <typename RequestType, typename ReplyType>
    static grpc::Status request_sleeper(grpc::ServerContext* context, const RequestType* request,
                                        grpc::ServerWriter<ReplyType>* response)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return grpc::Status::OK;
    }
};

TEST_P(TimeoutSuite, command_exits_on_timeout)
{
    auto [mock_utils, guard] = mpt::MockUtils::inject();

    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_CALL(*mock_utils, exit(mp::timeout_exit_code));

    send_command({GetParam(), "--timeout", "1"});
}

TEST_P(TimeoutSuite, command_completes_without_timeout)
{
    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));

    EXPECT_EQ(send_command({GetParam(), "--timeout", "5"}), mp::ReturnCode::Ok);
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutSuite, ValuesIn(timeout_commands));

struct ClientLogMessageSuite : Client, WithParamInterface<std::vector<std::string>>
{
    void SetUp() override
    {
        Client::SetUp();

        ON_CALL(mock_daemon, launch).WillByDefault(reply_log_message<mp::LaunchReply>);
        ON_CALL(mock_daemon, mount).WillByDefault(reply_log_message<mp::MountReply>);
        ON_CALL(mock_daemon, start).WillByDefault(reply_log_message<mp::StartReply>);
        ON_CALL(mock_daemon, version).WillByDefault(reply_log_message<mp::VersionReply>);
    }

    template <typename ReplyType>
    static grpc::Status reply_log_message(Unused, Unused, grpc::ServerWriter<ReplyType>* response)
    {
        ReplyType reply;
        reply.set_log_line(log_message);

        response->Write(reply);
        return grpc::Status{};
    }

    static constexpr auto log_message = "This is a fake log message";
};

TEST_P(ClientLogMessageSuite, clientPrintsOutExpectedLogMessage)
{
    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, mount).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, version).Times(AtMost(1));

    std::stringstream cerr_stream;

    send_command(GetParam(), trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), log_message);
}

INSTANTIATE_TEST_SUITE_P(Client, ClientLogMessageSuite,
                         Values(std::vector<std::string>{"launch"},
                                std::vector<std::string>{"mount", "..", "test-vm:test"},
                                std::vector<std::string>{"start"}, std::vector<std::string>{"version"}));

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

TEST_F(ClientAlias, aliases_help)
{
    std::stringstream cout_stream;
    send_command({"help", "aliases"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), HasSubstr("List available aliases"));
}
} // namespace
