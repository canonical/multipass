/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "mock_stdcin.h"
#include "path.h"
#include "stub_cert_store.h"
#include "stub_certprovider.h"
#include "stub_terminal.h"

#include <multipass/logging/log.h>
#include <src/client/client.h>
#include <src/daemon/daemon_rpc.h>

#include <QEventLoop>
#include <QStringList>
#include <QTemporaryFile>

#include <gmock/gmock.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct MockDaemonRpc : public mp::DaemonRpc
{
    using mp::DaemonRpc::DaemonRpc; // ctor

    MOCK_METHOD3(create, grpc::Status(grpc::ServerContext* context, const mp::CreateRequest* request,
                                      grpc::ServerWriter<mp::CreateReply>* reply));
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

#ifdef WIN32
    std::string server_address{"localhost:50051"};
#else
    std::string server_address{"unix:/tmp/test-multipassd.socket"};
#endif
    mpt::StubCertProvider cert_provider;
    mpt::StubCertStore cert_store;
    StrictMock<MockDaemonRpc> mock_daemon{server_address, mp::RpcConnectionType::insecure, cert_provider, cert_store}; // strict to fail on unexpected calls
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

// copy-files cli tests
TEST_F(Client, copy_files_cmd_good_source_remote)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"copy-files", "test-vm:foo", mpt::test_data_path().toStdString() + "good_index.json"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, copy_files_cmd_good_destination_remote)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _, _));
    EXPECT_THAT(send_command({"copy-files", mpt::test_data_path().toStdString() + "good_index.json", "test-vm:bar"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, copy_files_cmd_help_ok)
{
    EXPECT_THAT(send_command({"copy-files", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, copy_files_cmd_fails_invalid_source_file)
{
    EXPECT_THAT(send_command({"copy-files", "foo", "test-vm:bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, copy_files_cmd_fails_source_is_dir)
{
    EXPECT_THAT(send_command({"copy-files", mpt::test_data_path().toStdString(), "test-vm:bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, copy_files_cmd_fails_no_instance)
{
    EXPECT_THAT(send_command({"copy-files", mpt::test_data_path().toStdString() + "good_index.json", "."}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, copy_files_cmd_fails_instance_both_source_destination)
{
    EXPECT_THAT(send_command({"copy-files", "test-vm1:foo", "test-vm2:bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, copy_files_cmd_too_few_args_fails)
{
    EXPECT_THAT(send_command({"copy-files", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, copy_files_cmd_source_path_empty_fails)
{
    EXPECT_THAT(send_command({"copy-files", "test-vm1:", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, copy_file_cmd_multiple_sources_destination_file_fails)
{
    EXPECT_THAT(send_command({"copy-files", "test-vm1:foo", "test-vm2:bar",
                              mpt::test_data_path().toStdString() + "good_index.json"}),
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

// launch cli tests
TEST_F(Client, launch_cmd_good_arguments)
{
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

TEST_F(Client, launch_cmd_unknown_option_fails)
{
    EXPECT_THAT(send_command({"launch", "-z", "2"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_name_option_ok)
{
    EXPECT_THAT(send_command({"launch", "-n", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_name_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-n"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_memory_option_ok)
{
    EXPECT_THAT(send_command({"launch", "-m", "1G"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_memory_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-m"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_cpu_option_ok)
{
    EXPECT_THAT(send_command({"launch", "-c", "2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_cpu_option_fails_no_value)
{
    EXPECT_THAT(send_command({"launch", "-c"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launch_cmd_custom_image_file_ok)
{
    EXPECT_THAT(send_command({"launch", "file://foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launch_cmd_custom_image_http_ok)
{
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
    EXPECT_THAT(send_command({"launch", "--cloud-init", "-"}, trash_stream, trash_stream, ss), Eq(mp::ReturnCode::Ok));
}

// purge cli tests
TEST_F(Client, empty_trash_cmd_ok_no_args)
{
    EXPECT_THAT(send_command({"purge"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, empty_trash_cmd_fails_with_args)
{
    EXPECT_THAT(send_command({"purge", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, empty_trash_cmd_help_ok)
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
    EXPECT_THAT(send_command({"info", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"info", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_help_ok)
{
    EXPECT_THAT(send_command({"info", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_succeeds_with_all)
{
    EXPECT_THAT(send_command({"info", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, info_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"info", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

// list cli tests
TEST_F(Client, list_cmd_ok_no_args)
{
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
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "test-vm:test"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_good_relative_source_path)
{
    EXPECT_THAT(send_command({"mount", "..", "test-vm:test"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_fails_invalid_source_path)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path_for("foo").toStdString(), "test-vm:test"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mount_cmd_good_valid_uid_map)
{
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-u", "1000:501", "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_good_valid_large_uid_map)
{
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
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "-g", "1000:501", "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mount_cmd_good_valid_large_gid_map)
{
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
    EXPECT_THAT(send_command({"recover", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"recover", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_help_ok)
{
    EXPECT_THAT(send_command({"recover", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_succeeds_with_all)
{
    EXPECT_THAT(send_command({"recover", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recover_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"recover", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

// start cli tests
TEST_F(Client, start_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, start_cmd_ok_with_one_arg)
{
    EXPECT_THAT(send_command({"start", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"start", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_help_ok)
{
    EXPECT_THAT(send_command({"start", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_succeeds_with_all)
{
    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, start_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"start", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

// stop cli tests
TEST_F(Client, stop_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_ok_with_one_arg)
{
    EXPECT_THAT(send_command({"stop", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"stop", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_help_ok)
{
    EXPECT_THAT(send_command({"stop", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_succeeds_with_all)
{
    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"stop", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_fails_with_time_and_cancel)
{
    EXPECT_THAT(send_command({"stop", "--time", "+10", "--cancel", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stop_cmd_succeeds_with_plus_time)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+10"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stop_cmd_succeeds_with_no_plus_time)
{
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
    EXPECT_THAT(send_command({"stop", "foo", "--cancel"}), Eq(mp::ReturnCode::Ok));
}

// suspend cli tests
TEST_F(Client, suspend_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, suspend_cmd_ok_with_one_arg)
{
    EXPECT_THAT(send_command({"suspend", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"suspend", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_help_ok)
{
    EXPECT_THAT(send_command({"suspend", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_succeeds_with_all)
{
    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspend_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"suspend", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

// restart cli tests
TEST_F(Client, restart_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, restart_cmd_ok_with_one_arg)
{
    EXPECT_THAT(send_command({"restart", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"restart", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_help_ok)
{
    EXPECT_THAT(send_command({"restart", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restart_cmd_succeeds_with_all)
{
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

// trash cli tests
TEST_F(Client, trash_cmd_fails_no_args)
{
    EXPECT_THAT(send_command({"delete"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, trash_cmd_ok_with_one_arg)
{
    EXPECT_THAT(send_command({"delete", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, trash_cmd_succeeds_with_multiple_args)
{
    EXPECT_THAT(send_command({"delete", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, trash_cmd_help_ok)
{
    EXPECT_THAT(send_command({"delete", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, trash_cmd_succeeds_with_all)
{
    EXPECT_THAT(send_command({"delete", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, trash_cmd_fails_with_names_and_all)
{
    EXPECT_THAT(send_command({"delete", "--all", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, delete_cmd_accepts_purge_option)
{
    EXPECT_THAT(send_command({"delete", "--purge", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"delete", "-p", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, help_returns_ok_return_code)
{
    EXPECT_THAT(send_command({"--help"}), Eq(mp::ReturnCode::Ok));
}

// general help tests
TEST_F(Client, command_help_is_different_than_general_help)
{
    std::stringstream general_help_output;
    send_command({"--help"}, general_help_output);

    std::stringstream command_output;
    send_command({"list", "--help"}, command_output);

    EXPECT_THAT(general_help_output.str(), Ne(command_output.str()));
}

TEST_F(Client, help_cmd_create_same_create_cmd_help)
{
    std::stringstream help_cmd_create;
    send_command({"help", "launch"}, help_cmd_create);

    std::stringstream create_cmd_help;
    send_command({"launch", "-h"}, create_cmd_help);

    EXPECT_THAT(help_cmd_create.str(), Ne(""));
    EXPECT_THAT(help_cmd_create.str(), Eq(create_cmd_help.str()));
}
}
