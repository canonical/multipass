/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include "mock_settings.h"
#include "mock_stdcin.h"
#include "path.h"
#include "stub_cert_store.h"
#include "stub_certprovider.h"
#include "stub_terminal.h"

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/logging/log.h>
#include <src/client/cli/client.h>
#include <src/daemon/daemon_rpc.h>

#include <QEventLoop>
#include <QStringList>
#include <QTemporaryFile>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
auto petenv_name()
{
    return mp::Settings::instance().get(mp::petenv_key).toStdString();
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
    MOCK_METHOD3(ping, grpc::Status(grpc::ServerContext* context, const mp::PingRequest* request, mp::PingReply* response));
};

struct Client : public Test
{
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

    std::string get_setting(const std::string& key)
    {
        auto out = std::ostringstream{};
        EXPECT_THAT(send_command({"get", key}, out), Eq(mp::ReturnCode::Ok));

        auto ret = out.str();
        ret.pop_back(); // drop newline
        return ret;
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

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_ssh_info_matcher, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, petenv_launch_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_ssh_info_matcher, _)).WillOnce(Return(ok));

    EXPECT_THAT(send_command({"shell", petenv_name()}), Eq(mp::ReturnCode::Ok));
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

TEST_F(Client, launch_cmd_cpu_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-c"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_custom_image_file_ok)
{
    EXPECT_CALL(mock_daemon, launch(_, _, _));
    EXPECT_THAT(send_command({"launch", "file://foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_custom_image_http_ok)
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
    EXPECT_THAT(send_command({"launch", "--cloud-init", "/definitely/missing-file"}),
                Eq(mp::ReturnCode::CommandLineError));
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
    EXPECT_CALL(mock_daemon, list(_, _, _));
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
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher2, _)).Times(2);
    EXPECT_CALL(mock_daemon, start(_, petenv_matcher4, _));
    EXPECT_THAT(send_command({"start", "foo", petenv_name()}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", petenv_name(), "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", "foo", petenv_name(), "bar", "baz"}), Eq(mp::ReturnCode::Ok));
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

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, petenv_start_matcher, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, petenv_launch_matcher, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, petenv_start_matcher, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", petenv_name()}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_launches_petenv_if_absent_among_others_present)
{
    std::vector<std::string> instances{"a", "b", petenv_name(), "c"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher = make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name());
    const grpc::Status ok{}, aborted = aborted_start_status({petenv_name()});

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
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher2, _)).Times(2);
    EXPECT_CALL(mock_daemon, stop(_, petenv_matcher4, _));
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

// suspend cli tests
TEST_F(Client, suspend_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, suspend(_, _, _));
    EXPECT_THAT(send_command({"suspend", "foo"}), Eq(mp::ReturnCode::Ok));
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

// restart cli tests
TEST_F(Client, restart_cmd_ok_with_one_arg)
{
    EXPECT_CALL(mock_daemon, restart(_, _, _));
    EXPECT_THAT(send_command({"restart", "foo"}), Eq(mp::ReturnCode::Ok));
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
TEST_F(Client, get_can_read_settings)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key)));
    get_setting(mp::petenv_key);
}

TEST_F(Client, set_can_write_settings)
{
    const auto key = mp::petenv_key;
    const auto val = "blah";

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", key, val}), Eq(mp::ReturnCode::Ok));
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
    EXPECT_THAT(send_command({"set", key, val}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, get_handles_persistent_settings_errors)
{
    const auto key = mp::petenv_key;
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Throw(mp::PersistentSettingsException{"op", "test"}));
    EXPECT_THAT(send_command({"get", key}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, set_handles_persistent_settings_errors)
{
    const auto key = mp::petenv_key;
    const auto val = "asdasdasd";
    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val))).WillOnce(Throw(mp::PersistentSettingsException{"op", "test"}));
    EXPECT_THAT(send_command({"set", key, val}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, get_and_set_can_read_and_write_primary_name)
{
    const auto name = "xyz";
    const auto petenv_matcher = make_ssh_info_instance_matcher(name);

    EXPECT_THAT(get_setting(mp::petenv_key), AllOf(Not(IsEmpty()), StrNe(name)));

    EXPECT_CALL(mock_settings, set(Eq(mp::petenv_key), Eq(name)));
    EXPECT_THAT(send_command({"set", mp::petenv_key, name}), Eq(mp::ReturnCode::Ok));

    // EXPECT_THAT(get_setting(mp::petenv_key), StrEq(name));

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

    EXPECT_THAT(send_command({"set", mp::petenv_key, default_name}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(get_setting(mp::petenv_key), Eq(default_name));
}

TEST_F(Client, set_cmd_rejects_bad_primary_name)
{
    const auto default_name = get_setting(mp::petenv_key);
    const auto petenv_matcher = make_ssh_info_instance_matcher(default_name);
    const auto key = mp::petenv_key;
    const auto val = "123.badname_";

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
        .WillRepeatedly(Throw(mp::InvalidSettingsException{key, val, "bad"}));
    EXPECT_THAT(send_command({"set", key, val}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(get_setting(mp::petenv_key), Eq(default_name));

    EXPECT_CALL(mock_daemon, ssh_info(_, petenv_matcher, _));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
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
}
