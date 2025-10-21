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

#include "mock_ssh_test_fixture.h"

#include "common.h"
#include "disabling_macros.h"
#include "fake_alias_config.h"
#include "fake_key_data.h"
#include "mock_cert_provider.h"
#include "mock_environment_helpers.h"
#include "mock_file_ops.h"
#include "mock_network.h"
#include "mock_platform.h"
#include "mock_settings.h"
#include "mock_sftp_client.h"
#include "mock_sftp_utils.h"
#include "mock_standard_paths.h"
#include "mock_stdcin.h"
#include "mock_terminal.h"
#include "mock_utils.h"
#include "path.h"
#include "stub_cert_store.h"
#include "stub_console.h"
#include "stub_terminal.h"

#include <src/client/cli/client.h>
#include <src/client/cli/cmd/remote_settings_handler.h>
#include <src/daemon/daemon_rpc.h>

#include <multipass/cli/client_platform.h>
#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>

#include <QStringList>
#include <QTemporaryFile>
#include <QTimer>
#include <QtCore/QTemporaryDir>
#include <QtGlobal>

#include <chrono>
#include <initializer_list>
#include <thread>
#include <utility>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;
namespace mpt = multipass::test;
namespace mpu = multipass::utils;
using namespace testing;

namespace
{
struct MockDaemonRpc : public mp::DaemonRpc
{
    using mp::DaemonRpc::DaemonRpc; // ctor

    MOCK_METHOD(grpc::Status,
                create,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::CreateReply, mp::CreateRequest> * server)),
                (override)); // here only to ensure not called
    MOCK_METHOD(grpc::Status,
                launch,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::LaunchReply, mp::LaunchRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                purge,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::PurgeReply, mp::PurgeRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                find,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::FindReply, mp::FindRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                info,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::InfoReply, mp::InfoRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                list,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::ListReply, mp::ListRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                mount,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::MountReply, mp::MountRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                recover,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::RecoverReply, mp::RecoverRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                ssh_info,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                start,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::StartReply, mp::StartRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                stop,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::StopReply, mp::StopRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                suspend,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::SuspendReply, mp::SuspendRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                restart,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::RestartReply, mp::RestartRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                delet,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::DeleteReply, mp::DeleteRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                umount,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::UmountReply, mp::UmountRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                version,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::VersionReply, mp::VersionRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                ping,
                (grpc::ServerContext * context,
                 const mp::PingRequest* request,
                 mp::PingReply* response),
                (override));
    MOCK_METHOD(grpc::Status,
                get,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::GetReply, mp::GetRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                authenticate,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::AuthenticateReply, mp::AuthenticateRequest> *
                  server)),
                (override));
    MOCK_METHOD(grpc::Status,
                snapshot,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::SnapshotReply, mp::SnapshotRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                restore,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::RestoreReply, mp::RestoreRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                clone,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::CloneReply, mp::CloneRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                wait_ready,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::WaitReadyReply, mp::WaitReadyRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                zones,
                (grpc::ServerContext * context, (grpc::ServerReaderWriter<mp::ZonesReply, mp::ZonesRequest> * server)),
                (override));
    MOCK_METHOD(grpc::Status,
                zones_state,
                (grpc::ServerContext * context,
                 (grpc::ServerReaderWriter<mp::ZonesStateReply, mp::ZonesStateRequest> * server)),
                (override));
};

struct Client : public Test
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(petenv_name));
        EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillRepeatedly(Return("none"));
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));
        EXPECT_CALL(mock_settings, register_handler(_)).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(*mock_utils, contents_of(_)).WillRepeatedly(Return(mpt::root_cert));

        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), locate(_, _, _))
            .Times(AnyNumber()); // needed to allow general calls once we have added the specific
                                 // expectation below
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                    locate(_, mpt::match_qstring(EndsWith("settings.json")), _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(
                "")); /* Avoid writing to Windows Terminal settings. We use an "expectation" so that
                         it gets reset at the end of each test (by VerifyAndClearExpectations) */
    }

    void TearDown() override
    {
        testing::Mock::VerifyAndClearExpectations(
            &mock_daemon); /* We got away without this before because, being a
                     strict mock every call to mock_daemon had to be explicitly
                     "expected". Being the best match for incoming calls, each
                     expectation took precedence over the previous ones,
                     preventing them from being saturated inadvertently */
    }

    int setup_client_and_run(const std::vector<std::string>& command, mp::Terminal& term)
    {
        mp::ClientConfig client_config{server_address, get_client_cert_provider(), &term};
        mp::Client client{client_config};
        QStringList args = QStringList() << "multipass_test";

        for (const auto& arg : command)
        {
            args << QString::fromStdString(arg);
        }
        return client.run(args);
    }

    int send_command(const std::vector<std::string>& command,
                     std::ostream& cout = trash_stream,
                     std::ostream& cerr = trash_stream,
                     std::istream& cin = trash_stream)
    {
        mpt::StubTerminal term(cout, cerr, cin);
        return setup_client_and_run(command, term);
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

    static auto make_mount_matcher(const std::string_view fake_source,
                                   const std::string_view fake_target,
                                   const std::string_view instance_name)
    {
        const auto automount_source_matcher =
            Property(&mp::MountRequest::source_path, StrEq(fake_source));

        const auto target_instance_matcher =
            Property(&mp::TargetPathInfo::instance_name, StrEq(instance_name));
        const auto target_path_matcher =
            Property(&mp::TargetPathInfo::target_path, StrEq(fake_target));
        const auto target_info_matcher = AllOf(target_instance_matcher, target_path_matcher);
        const auto automount_target_matcher =
            Property(&mp::MountRequest::target_paths,
                     AllOf(Contains(target_info_matcher), SizeIs(1)));

        return AllOf(automount_source_matcher, automount_target_matcher);
    }

    static auto make_mount_id_mappings_matcher(const mp::id_mappings& expected_uid_mappings,
                                               const mp::id_mappings& expected_gid_mappings)
    {
        auto compare_uid_map = [expected_uid_mappings](auto request_uid_mappings) {
            auto mappings_size = expected_uid_mappings.size();
            if (mappings_size != (decltype(mappings_size))request_uid_mappings.size())
                return false;

            for (decltype(mappings_size) i = 0; i < mappings_size; ++i)
                if (expected_uid_mappings[i].first != request_uid_mappings.at(i).host_id() ||
                    expected_uid_mappings[i].second != request_uid_mappings.at(i).instance_id())
                    return false;

            return true;
        };

        const auto uid_map_matcher =
            Property(&mp::MountRequest::mount_maps,
                     Property(&mp::MountMaps::uid_mappings, ResultOf(compare_uid_map, true)));

        auto compare_gid_map = [expected_gid_mappings](auto request_gid_mappings) {
            auto mappings_size = expected_gid_mappings.size();
            if (mappings_size != (decltype(mappings_size))request_gid_mappings.size())
                return false;

            for (decltype(mappings_size) i = 0; i < mappings_size; ++i)
                if (expected_gid_mappings[i].first != request_gid_mappings.at(i).host_id() ||
                    expected_gid_mappings[i].second != request_gid_mappings.at(i).instance_id())
                    return false;

            return true;
        };

        const auto gid_map_matcher =
            Property(&mp::MountRequest::mount_maps,
                     Property(&mp::MountMaps::gid_mappings, ResultOf(compare_gid_map, true)));

        return AllOf(uid_map_matcher, gid_map_matcher);
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
        return Property(&RequestType::instance_names,
                        Property(&mp::InstanceNames::instance_name, instances_matcher));
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
        return make_instances_matcher<RequestType>(
            AllOf(Contains(StrEq(instance_name)), SizeIs(size)));
    }

    template <typename RequestType>
    auto make_request_verbosity_matcher(
        decltype(std::declval<RequestType>().verbosity_level()) verbosity)
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
        EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
            .WillRepeatedly(Throw(mp::InvalidSettingException{key, val, "bad"}));
        EXPECT_THAT(send_command({"set", keyval_arg(key, val)}),
                    Eq(mp::ReturnCode::CommandLineError));
    }

    auto make_fill_listreply(std::vector<mp::InstanceStatus_Status> statuses)
    {
        return
            [statuses](Unused, grpc::ServerReaderWriter<mp::ListReply, mp::ListRequest>* response) {
                mp::ListReply list_reply;

                for (mp::InstanceStatus_Status status : statuses)
                {
                    auto list_entry = list_reply.mutable_instance_list()->add_instances();
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

    template <typename ReplyType, typename RequestType, typename Matcher>
    auto check_request_and_return(const Matcher& matcher,
                                  const grpc::Status& status,
                                  const ReplyType& reply = ReplyType{})
    {
        return [&matcher, &status, reply = std::move(reply)](
                   grpc::ServerReaderWriter<ReplyType, RequestType>* server) {
            RequestType request;
            server->Read(&request);

            EXPECT_THAT(request, matcher);

            server->Write(std::move(reply));
            return status;
        };
    }

#ifdef WIN32
    std::string server_address{"localhost:50051"};
#else
    std::string server_address{"unix:/tmp/test-multipassd.socket"};
#endif
    static std::unique_ptr<NiceMock<mpt::MockCertProvider>> get_client_cert_provider()
    {
        return std::make_unique<NiceMock<mpt::MockCertProvider>>();
    };
    std::unique_ptr<NiceMock<mpt::MockCertProvider>> daemon_cert_provider{
        std::make_unique<NiceMock<mpt::MockCertProvider>>()};
    const mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    const mpt::MockPlatform* mock_platform = platform_attr.first;
    const mpt::MockUtils::GuardedMock utils_attr{mpt::MockUtils::inject<NiceMock>()};
    const mpt::MockUtils* mock_utils = utils_attr.first;

    mpt::StubCertStore cert_store;
    StrictMock<MockDaemonRpc> mock_daemon{
        server_address,
        *daemon_cert_provider,
        &cert_store}; // strict to fail on unexpected calls and play well with sharing
    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
    inline static std::stringstream
        trash_stream; // this may have contents (that we don't care about)
    static constexpr char petenv_name[] = "the-petenv";
    const grpc::Status ok{};

    mpt::MockSSHTestFixture mock_ssh_test_fixture;
};

struct ClientAlias : public Client, public FakeAliasConfig
{
    ClientAlias()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(fake_alias_dir.path()));

        EXPECT_CALL(*mock_platform, create_alias_script(_, _)).WillRepeatedly(Return());
        EXPECT_CALL(*mock_platform, remove_alias_script(_)).WillRepeatedly(Return());
    }
};

auto make_info_function(const std::string& source_path = "", const std::string& target_path = "")
{
    auto info_function = [&source_path, &target_path](
                             grpc::ServerContext*,
                             grpc::ServerReaderWriter<mp::InfoReply, mp::InfoRequest>* server) {
        mp::InfoRequest request;
        server->Read(&request);

        mp::InfoReply info_reply;

        if (request.instance_snapshot_pairs(0).instance_name() == "primary")
        {
            auto vm_info = info_reply.add_details();
            vm_info->set_name("primary");
            vm_info->mutable_instance_status()->set_status(mp::InstanceStatus::RUNNING);

            if (!source_path.empty() && !target_path.empty())
            {
                auto mount_info = vm_info->mutable_mount_info();

                auto entry = mount_info->add_mount_paths();
                entry->set_source_path(source_path);
                entry->set_target_path(target_path);

                if (source_path.size() > target_path.size())
                    mount_info->set_longest_path_len(source_path.size());
                else
                    mount_info->set_longest_path_len(target_path.size());
            }

            server->Write(info_reply);

            return grpc::Status{};
        }
        else
            return grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
    };

    return info_function;
}

mp::SSHInfo make_ssh_info(const std::string& host = "222.222.222.222",
                          int port = 22,
                          const std::string& priv_key = mpt::fake_key_data,
                          const std::string& username = "user")
{
    mp::SSHInfo ssh_info;

    ssh_info.set_host(host);
    ssh_info.set_port(port);
    ssh_info.set_priv_key_base64(priv_key);
    ssh_info.set_username(username);

    return ssh_info;
}

mp::SSHInfoReply make_fake_ssh_info_response(const std::string& instance_name)
{
    mp::SSHInfoReply response;
    (*response.mutable_ssh_info())[instance_name] = make_ssh_info();

    return response;
}

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

const std::string csv_header{"Alias,Instance,Command,Working directory,Context\n"};

// Tests for no postional args given
TEST_F(Client, noCommandIsError)
{
    EXPECT_THAT(send_command({}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, noCommandHelpOk)
{
    EXPECT_THAT(send_command({"-h"}), Eq(mp::ReturnCode::Ok));
}

// Remote-handler tests
template <typename M>
auto match_uptr_to_remote_settings_handler(M&& matcher)
{
    return Pointee(Address(WhenDynamicCastTo<const mp::RemoteSettingsHandler*>(
        AllOf(NotNull(), std::forward<M>(matcher)))));
}

struct RemoteHandlerTest : public Client, public WithParamInterface<std::string>
{
    inline static const auto cmds = Values("", " ", "help", "get");
};

TEST_P(RemoteHandlerTest, registersRemoteSettingsHandler)
{
    EXPECT_CALL(mock_settings, // clang-format don't get it
                register_handler(match_uptr_to_remote_settings_handler(
                    Property(&mp::RemoteSettingsHandler::get_key_prefix, Eq("local.")))))
        .Times(1);

    send_command({GetParam()});
}

TEST_P(RemoteHandlerTest, unregistersRemoteSettingsHandler)
{
    auto handler = reinterpret_cast<mp::SettingsHandler*>(0x123123);
    EXPECT_CALL(mock_settings, register_handler(match_uptr_to_remote_settings_handler(_)))
        .WillOnce(Return(handler));
    EXPECT_CALL(mock_settings, unregister_handler(handler)).Times(1);

    send_command({GetParam()});
}

INSTANTIATE_TEST_SUITE_P(Client, RemoteHandlerTest, RemoteHandlerTest::cmds);

struct RemoteHandlerVerbosity : public Client, WithParamInterface<std::tuple<int, std::string>>
{
};

TEST_P(RemoteHandlerVerbosity, honorsVerbosityInRemoteSettingsHandler)
{
    const auto& [num_vs, cmd] = GetParam();
    EXPECT_CALL(mock_settings, // clang-format hands off...
                register_handler(match_uptr_to_remote_settings_handler(
                    Property(&mp::RemoteSettingsHandler::get_verbosity,
                             Eq(num_vs))))) // ... this piece of code
        .Times(1);

    auto vs = fmt::format("{}{}", num_vs ? "-" : "", std::string(num_vs, 'v'));
    send_command({vs, cmd});
}

INSTANTIATE_TEST_SUITE_P(Client,
                         RemoteHandlerVerbosity,
                         Combine(Range(0, 5), RemoteHandlerTest::cmds));

TEST_F(Client, handlesRemoteHandlerException)
{
    auto cmd = "get";
    auto key = "nowhere";
    auto msg = "can't";
    auto details = "too far";

    EXPECT_CALL(mock_settings, get(QString{key}))
        .WillOnce(Throw(mp::RemoteHandlerException{
            grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, msg, details}}));

    std::stringstream fake_cerr;
    auto got = send_command({cmd, key}, trash_stream, fake_cerr);

    EXPECT_THAT(fake_cerr.str(), AllOf(HasSubstr(cmd), HasSubstr(msg), Not(HasSubstr(details))));
    EXPECT_EQ(got, mp::CommandFail);
}

// transfer cli tests
TEST_F(Client, transferCmdInstanceSourceLocalTarget)
{
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();
    auto mocked_sftp_client = std::make_unique<mpt::MockSFTPClient>();
    auto mocked_sftp_client_p = mocked_sftp_client.get();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::move(mocked_sftp_client)));
    EXPECT_CALL(*mocked_sftp_client_p, pull).WillOnce(Return(true));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });
    EXPECT_EQ(send_command({"transfer", "test-vm:foo", "bar"}), mp::ReturnCode::Ok);
}

TEST_F(Client, transferCmdInstanceSourcesLocalTargetNotDir)
{
    auto [mocked_file_ops, mocked_file_ops_guard] = mpt::MockFileOps::inject();
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::make_unique<mpt::MockSFTPClient>()));
    EXPECT_CALL(*mocked_file_ops, is_directory).WillOnce(Return(false));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });

    std::stringstream err;
    EXPECT_EQ(send_command({"transfer", "test-vm:foo", "test-vm:baz", "bar"}, trash_stream, err),
              mp::ReturnCode::CommandFail);
    EXPECT_THAT(err.str(), HasSubstr("Target \"bar\" is not a directory"));
}

TEST_F(Client, transferCmdInstanceSourcesLocalTargetCannotAccess)
{
    auto [mocked_file_ops, mocked_file_ops_guard] = mpt::MockFileOps::inject();
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::make_unique<mpt::MockSFTPClient>()));
    auto err = std::make_error_code(std::errc::permission_denied);
    EXPECT_CALL(*mocked_file_ops, is_directory).WillOnce([&](auto, std::error_code& e) {
        e = err;
        return false;
    });
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });

    std::stringstream err_sink;
    EXPECT_EQ(
        send_command({"transfer", "test-vm:foo", "test-vm:baz", "bar"}, trash_stream, err_sink),
        mp::ReturnCode::CommandFail);
    EXPECT_THAT(err_sink.str(), HasSubstr(fmt::format("Cannot access \"bar\": {}", err.message())));
}

TEST_F(Client, transferCmdLocalSourcesInstanceTargetNotDir)
{
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();
    auto mocked_sftp_client = std::make_unique<mpt::MockSFTPClient>();
    auto mocked_sftp_client_p = mocked_sftp_client.get();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::move(mocked_sftp_client)));
    EXPECT_CALL(*mocked_sftp_client_p, is_remote_dir).WillOnce(Return(false));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });

    std::stringstream err;
    EXPECT_EQ(send_command({"transfer", "foo", "baz", "test-vm:bar"}, trash_stream, err),
              mp::ReturnCode::CommandFail);
    EXPECT_THAT(err.str(), HasSubstr("Target \"bar\" is not a directory"));
}

TEST_F(Client, transferCmdLocalSourceInstanceTarget)
{
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();
    auto mocked_sftp_client = std::make_unique<mpt::MockSFTPClient>();
    auto mocked_sftp_client_p = mocked_sftp_client.get();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::move(mocked_sftp_client)));
    EXPECT_CALL(*mocked_sftp_client_p, push).WillRepeatedly(Return(true));
    EXPECT_CALL(*mocked_sftp_client_p, is_remote_dir).WillOnce(Return(true));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });

    EXPECT_EQ(send_command({"transfer", "foo", "C:\\Users\\file", "test-vm:bar"}),
              mp::ReturnCode::Ok);
}

TEST_F(Client, transferCmdHelpOk)
{
    EXPECT_THAT(send_command({"transfer", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, transferCmdFailsNoInstance)
{
    EXPECT_THAT(send_command({"transfer", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdFailsInstanceBothSourceDestination)
{
    EXPECT_THAT(send_command({"transfer", "test-vm1:foo", "test-vm2:bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdFailsTooFewArgs)
{
    EXPECT_THAT(send_command({"transfer", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdLocalTargetNotAllInstanceSourcesFails)
{
    EXPECT_THAT(send_command({"transfer", "aaa", "test-vm1:foo", "bbb"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdStdinGoodDestinationOk)
{
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();
    auto mocked_sftp_client = std::make_unique<mpt::MockSFTPClient>();
    auto mocked_sftp_client_p = mocked_sftp_client.get();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::move(mocked_sftp_client)));
    EXPECT_CALL(*mocked_sftp_client_p, from_cin);
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });

    EXPECT_EQ(send_command({"transfer", "-", "test-vm1:foo"}), mp::ReturnCode::Ok);
}

TEST_F(Client, transferCmdStdinBadDestinationFails)
{
    EXPECT_THAT(send_command({"transfer", "-", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdStdoutGoodSourceOk)
{
    auto [mocked_sftp_utils, mocked_sftp_utils_guard] = mpt::MockSFTPUtils::inject();
    auto mocked_sftp_client = std::make_unique<mpt::MockSFTPClient>();
    auto mocked_sftp_client_p = mocked_sftp_client.get();

    EXPECT_CALL(*mocked_sftp_utils, make_SFTPClient)
        .WillOnce(Return(std::move(mocked_sftp_client)));
    EXPECT_CALL(*mocked_sftp_client_p, to_cout);
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            mp::SSHInfoReply reply;
            reply.mutable_ssh_info()->insert({"test-vm", mp::SSHInfo{}});
            server->Write(reply);
            return grpc::Status{};
        });

    EXPECT_EQ(send_command({"transfer", "test-vm1:foo", "-"}), mp::ReturnCode::Ok);
}

TEST_F(Client, transferCmdStdoutBadSourceFails)
{
    EXPECT_THAT(send_command({"transfer", "foo", "-"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdStdoutStdinOnlyFails)
{
    EXPECT_THAT(send_command({"transfer", "-", "-"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdStdoutStdinDeclarationFails)
{
    EXPECT_THAT(send_command({"transfer", "test-vm1:foo", "-", "-"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, transferCmdStreamTooManyArgs)
{
    EXPECT_THAT(send_command({"transfer", "test-vm1:foo", "aaaaa", "-"}),
                Eq(mp::ReturnCode::CommandLineError));
}

// shell cli test
TEST_F(Client, shellCmdGoodArguments)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _));
    EXPECT_THAT(send_command({"shell", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdHelpOk)
{
    EXPECT_THAT(send_command({"shell", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdNoArgsTargetsPetenv)
{
    const auto petenv_matcher = make_ssh_info_instance_matcher(petenv_name);
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdCreatesConsole)
{
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce([](auto, grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            server->Write(make_fake_ssh_info_response("fake-instance"));
            return grpc::Status{};
        });

    std::string error_string = "attempted to create console";
    std::stringstream cerr_stream;

    mpt::MockTerminal term{};
    EXPECT_CALL(term, cin()).WillRepeatedly(ReturnRef(trash_stream));
    EXPECT_CALL(term, cout()).WillRepeatedly(ReturnRef(trash_stream));
    EXPECT_CALL(term, cerr()).WillRepeatedly(ReturnRef(cerr_stream));
    EXPECT_CALL(term, make_console(_)).WillOnce(Throw(std::runtime_error(error_string)));

    EXPECT_EQ(setup_client_and_run({"shell"}, term), mp::ReturnCode::CommandFail);
    EXPECT_THAT(cerr_stream.str(), HasSubstr(error_string));
}

TEST_F(Client, shellCmdConsidersConfiguredPetenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_ssh_info_instance_matcher(custom_petenv);
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdCanTargetPetenvExplicitly)
{
    const auto petenv_matcher = make_ssh_info_instance_matcher(petenv_name);
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdLaunchesPetenvIfAbsent)
{
    const auto petenv_ssh_info_matcher = make_ssh_info_instance_matcher(petenv_name);
    const auto petenv_launch_matcher =
        Property(&mp::LaunchRequest::instance_name, StrEq(petenv_name));
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, mount).WillRepeatedly(Return(ok)); // 0 or more times

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(petenv_ssh_info_matcher,
                                                                           notfound)));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(petenv_launch_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(petenv_ssh_info_matcher,
                                                                           ok)));

    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdAutomountsWhenLaunchingPetenv)
{
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info(_, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdSkipsAutomountWhenDisabled)
{
    std::stringstream cout_stream;
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillOnce(Return("false"));
    EXPECT_CALL(mock_daemon, mount(_, _)).Times(0);
    EXPECT_CALL(mock_daemon, ssh_info(_, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", petenv_name}, cout_stream), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping mount due to disabled mounts feature\n"));
}

TEST_F(Client, shellCmdForwardsVerbosityToSubcommands)
{
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};
    const auto verbosity = 3;
    const auto ssh_info_verbosity_matcher =
        make_request_verbosity_matcher<mp::SSHInfoRequest>(verbosity);
    const auto launch_verbosity_matcher =
        make_request_verbosity_matcher<mp::LaunchRequest>(verbosity);
    const auto mount_verbosity_matcher =
        make_request_verbosity_matcher<mp::MountRequest>(verbosity);

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(
            ssh_info_verbosity_matcher,
            notfound)));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_verbosity_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(mount_verbosity_matcher,
                                                                       ok)));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(
            ssh_info_verbosity_matcher,
            ok)));
    EXPECT_THAT(send_command({"shell", "-vvv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdForwardsTimeoutToSubcommands)
{
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};
    const auto timeout = 123;
    const auto launch_timeout_matcher = make_request_timeout_matcher<mp::LaunchRequest>(timeout);

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_timeout_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, mount).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, ssh_info).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"shell", "--timeout", std::to_string(timeout)}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"},
        error{grpc::StatusCode::INTERNAL, "oops"};
    const auto except = mp::RemoteHandlerException{error};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillOnce(Throw(except));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shellCmdFailsWhenAutomountingInPetenvFails)
{
    const auto notfound = grpc::Status{grpc::StatusCode::NOT_FOUND, "msg"};
    const auto mount_failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info(_, _)).WillOnce(Return(notfound));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shellCmdStartsInstanceIfStoppedOrSuspended)
{
    const auto instance = "ordinary";
    const auto ssh_info_matcher = make_ssh_info_instance_matcher(instance);
    const auto start_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(instance);
    const grpc::Status aborted{grpc::StatusCode::ABORTED, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(ssh_info_matcher,
                                                                           aborted)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_matcher, ok)));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(ssh_info_matcher, ok)));

    EXPECT_THAT(send_command({"shell", instance}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdStartsPetenvIfStoppedOrSuspended)
{
    const auto ssh_info_matcher = make_ssh_info_instance_matcher(petenv_name);
    const auto start_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name);
    const grpc::Status aborted{grpc::StatusCode::ABORTED, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(ssh_info_matcher,
                                                                           aborted)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_matcher, ok)));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(ssh_info_matcher, ok)));

    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdFailsIfPetenvPresentButDeleted)
{
    const auto petenv_matcher = make_ssh_info_instance_matcher(petenv_name);
    const grpc::Status failed_precond{grpc::StatusCode::FAILED_PRECONDITION, "msg"};

    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(petenv_matcher,
                                                                           failed_precond)));
    EXPECT_THAT(send_command({"shell", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shellCmdFailsOnOtherAbsentInstance)
{
    const auto instance = "ordinary";
    const auto instance_matcher = make_ssh_info_instance_matcher(instance);
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(instance_matcher,
                                                                           notfound)));
    EXPECT_THAT(send_command({"shell", instance}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, shellCmdFailsMultipleArgs)
{
    EXPECT_THAT(send_command({"shell", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, shellCmdFailsUnknownOptions)
{
    EXPECT_THAT(send_command({"shell", "--not", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, shellCmdDisabledPetenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_CALL(mock_daemon, ssh_info(_, _)).Times(0);
    EXPECT_THAT(send_command({"shell"}), Eq(mp::ReturnCode::CommandLineError));

    EXPECT_CALL(mock_daemon, ssh_info(_, _)).Times(2);
    EXPECT_THAT(send_command({"shell", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"shell", "primary"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, shellCmdDisabledPetenvHelp)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_CALL(mock_daemon, ssh_info(_, _)).Times(0);
    EXPECT_THAT(send_command({"shell", "-h"}), Eq(mp::ReturnCode::Ok));
}

// launch cli tests
TEST_F(Client, launchCmdGoodArguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdWrongMemArguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _)).Times(0);
    MP_EXPECT_THROW_THAT(send_command({"launch", "-m", "wrong"}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("wrong is not a valid memory size")));
    MP_EXPECT_THROW_THAT(send_command({"launch", "--memory", "1.23f"}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("1.23f is not a valid memory size")));
    MP_EXPECT_THROW_THAT(
        send_command({"launch", "-memory", "2048M"}),
        std::runtime_error,
        mpt::match_what(HasSubstr("emory is not a valid memory size"))); // note single dash
}

TEST_F(Client, launchCmdWrongDiskArguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _)).Times(0);
    MP_EXPECT_THROW_THAT(send_command({"launch", "-d", "wrong"}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("wrong is not a valid memory size")));
    MP_EXPECT_THROW_THAT(send_command({"launch", "--disk", "4.56f"}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("4.56f is not a valid memory size")));
    MP_EXPECT_THROW_THAT(
        send_command({"launch", "-disk", "8192M"}),
        std::runtime_error,
        mpt::match_what(HasSubstr("isk is not a valid memory size"))); // note single dash
}

TEST_F(Client, launchCmdHelpOk)
{
    EXPECT_THAT(send_command({"launch", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdFailsMultipleArgs)
{
    EXPECT_THAT(send_command({"launch", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdFailsUnknownOption)
{
    EXPECT_THAT(send_command({"launch", "-z", "2"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdNameOptionOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "-n", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdNameOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "-n"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdMemoryOptionOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "-m", "1G"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdMemoryOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "-m"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "-c", "2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdCpuOptionAlphaNumericFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "w00t"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionAlphaFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "many"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionDecimalFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "1.608"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionZeroFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "0"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionNegativeFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "-2"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "-c"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, DISABLE_ON_MACOS(launchCmdCustomImageFileOk))
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "file://foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, DISABLE_ON_MACOS(launchCmdCustomImageHttpOk))
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "http://foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdWithZoneOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--zone", "zone1"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdWithEmptyZoneFails)
{
    EXPECT_THAT(send_command({"launch", "--zone", ""}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdWithInvalidZoneFails)
{
    const auto request_matcher = Property(&mp::LaunchRequest::zone, StrEq("invalid_zone"));
    mp::LaunchError launch_error;
    launch_error.add_error_codes(mp::LaunchError::INVALID_ZONE);
    const auto failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg", launch_error.SerializeAsString()};
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(request_matcher, failure)));
    EXPECT_THAT(send_command({"launch", "--zone", "invalid_zone"}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdWithUnavailableZoneFails)
{
    const auto request_matcher = Property(&mp::LaunchRequest::zone, StrEq("unavailable_zone"));
    mp::LaunchError launch_error;
    launch_error.add_error_codes(mp::LaunchError::ZONE_UNAVAILABLE);
    const auto failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg", launch_error.SerializeAsString()};
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(request_matcher, failure)));
    EXPECT_THAT(send_command({"launch", "--zone", "unavailable_zone"}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdWithTimer)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--timeout", "1"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdCloudinitOptionWithValidFileIsOk)
{
    QTemporaryFile tmpfile; // file is auto-deleted when this goes out of scope
    tmpfile.open();
    tmpfile.write("password: passw0rd"); // need some YAML
    tmpfile.close();
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--cloud-init", qPrintable(tmpfile.fileName())}),
                Eq(mp::ReturnCode::Ok));
}

struct BadCloudInitFile : public Client, WithParamInterface<std::string>
{
};

TEST_P(BadCloudInitFile, launchCmdCloudinitOptionFailsWithMissingFile)
{
    std::stringstream cerr_stream;
    auto missing_file{"/definitely/missing-file"};

    EXPECT_THAT(send_command({"launch", "--cloud-init", missing_file}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_NE(std::string::npos, cerr_stream.str().find("Could not load"))
        << "cerr has: " << cerr_stream.str();
    EXPECT_NE(std::string::npos, cerr_stream.str().find(missing_file))
        << "cerr has: " << cerr_stream.str();
}

INSTANTIATE_TEST_SUITE_P(Client,
                         BadCloudInitFile,
                         Values("/definitely/missing-file", "/dev/null", "/home", "/root/.bashrc"));

TEST_F(Client, launchCmdCloudinitOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "--cloud-init"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCloudinitOptionReadsStdinOk)
{
    MockStdCin cin("password: passw0rd"); // no effect since terminal encapsulation of streams

    std::stringstream ss;
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--cloud-init", "-"}, trash_stream, trash_stream, ss),
                Eq(mp::ReturnCode::Ok));
}

#ifndef WIN32 // TODO make home mocking work for windows
TEST_F(Client, launchCmdAutomountsHomeInPetenv)
{
    const auto fake_home = QTemporaryDir{}; // the client checks the mount source exists
    const auto env_scope = mpt::SetEnvScope{"HOME", fake_home.path().toUtf8()};
    const auto home_automount_matcher =
        make_mount_matcher(fake_home.path().toStdString(), mp::home_automount_dir, petenv_name);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name);

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(petenv_launch_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(home_automount_matcher,
                                                                       ok)));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name}), Eq(mp::ReturnCode::Ok));
}
#endif

TEST_F(Client, launchCmdSkipsAutomountWhenDisabled)
{
    std::stringstream cout_stream;
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillOnce(Return("false"));

    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount).Times(0);

    EXPECT_THAT(send_command({"launch", "--name", petenv_name}, cout_stream),
                Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping mount due to disabled mounts feature\n"));
}

TEST_F(Client, launchCmdOnlyWarnsMountForPetEnv)
{
    const auto invalid_argument = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
    std::stringstream cout_stream;
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("false"));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(invalid_argument));

    EXPECT_THAT(send_command({"launch", "--name", ".asdf"}, cout_stream),
                Eq(mp::ReturnCode::CommandFail));
    EXPECT_THAT(cout_stream.str(),
                Not(HasSubstr("Skipping mount due to disabled mounts feature\n")));
}

TEST_F(Client, launchCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const auto except =
        mp::RemoteHandlerException{grpc::Status{grpc::StatusCode::INTERNAL, "oops"}};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillOnce(Throw(except));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"launch", "--name", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdFailsWhenAutomountingInPetenvFails)
{
    const grpc::Status mount_failure{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdForwardsVerbosityToSubcommands)
{
    const auto verbosity = 4;
    const auto launch_verbosity_matcher =
        make_request_verbosity_matcher<mp::LaunchRequest>(verbosity);
    const auto mount_verbosity_matcher =
        make_request_verbosity_matcher<mp::MountRequest>(verbosity);

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_verbosity_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(mount_verbosity_matcher,
                                                                       ok)));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name, "-vvvv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdDoesNotAutomountInNormalInstances)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_CALL(mock_daemon, mount(_, _))
        .Times(0); // because we may want to move from a Strict mock in the future
    EXPECT_THAT(send_command({"launch"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdDisabledPetenvPasses)
{
    const auto custom_petenv = "";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_launch_instance_matcher("foo");
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(petenv_matcher, ok)));

    EXPECT_THAT(send_command({"launch", "--name", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, launchCmdMountOption)
{
    const QTemporaryDir fake_directory{};

    const auto fake_source = fake_directory.path().toStdString();
    const auto fake_target = "";
    const auto instance_name = "some_instance";

    const auto mount_matcher = make_mount_matcher(fake_source, fake_target, instance_name);
    const auto launch_matcher = make_launch_instance_matcher(instance_name);

    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_matcher, ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(mount_matcher, ok)));
    EXPECT_EQ(send_command({"launch", "--name", instance_name, "--mount", fake_source}),
              mp::ReturnCode::Ok);
}

TEST_F(Client, launchCmdMountOptionFailsOnInvalidDir)
{
    auto [mocked_file_ops, mocked_file_ops_guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mocked_file_ops, exists(A<const QFileInfo&>()))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mocked_file_ops, isDir(A<const QFileInfo&>()))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mocked_file_ops, isReadable(A<const QFileInfo&>()))
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));

    const auto fake_source = "invalid/dir";
    const auto fake_target = fake_source;
    const auto instance_name = "some_instance";

    std::stringstream err;
    EXPECT_EQ(send_command({"launch",
                            "--name",
                            instance_name,
                            "--mount",
                            fmt::format("{}:{}", fake_source, fake_target)},
                           trash_stream,
                           err),
              mp::ReturnCode::CommandLineError);
    EXPECT_THAT(err.str(),
                HasSubstr(fmt::format("Mount source path \"{}\" does not exist", fake_source)));
    err.clear();

    EXPECT_EQ(send_command({"launch",
                            "--name",
                            instance_name,
                            "--mount",
                            fmt::format("{}:{}", fake_source, fake_target)},
                           trash_stream,
                           err),
              mp::ReturnCode::CommandLineError);
    EXPECT_THAT(err.str(),
                HasSubstr(fmt::format("Mount source path \"{}\" is not a directory", fake_source)));
    err.clear();

    EXPECT_EQ(send_command({"launch",
                            "--name",
                            instance_name,
                            "--mount",
                            fmt::format("{}:{}", fake_source, fake_target)},
                           trash_stream,
                           err),
              mp::ReturnCode::CommandLineError);
    EXPECT_THAT(err.str(),
                HasSubstr(fmt::format("Mount source path \"{}\" is not readable", fake_source)));
}

TEST_F(Client, launchCmdPetenvMountOptionOverrideHome)
{
    const QTemporaryDir fake_directory{};

    const auto fake_source = fake_directory.path().toStdString();
    const auto fake_target = mp::home_automount_dir;

    const auto mount_matcher = make_mount_matcher(fake_source, fake_target, petenv_name);
    const auto launch_matcher = make_launch_instance_matcher(petenv_name);

    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_matcher, ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(mount_matcher, ok)));
    EXPECT_EQ(send_command({"launch",
                            "--name",
                            petenv_name,
                            "--mount",
                            fmt::format("{}:{}", fake_source, fake_target)}),
              mp::ReturnCode::Ok);
}

TEST_F(Client, launchCmdCloudinitUrl)
{
    const auto fake_url = QStringLiteral("https://example.com");
    const auto fake_downloaded_yaml = QByteArrayLiteral("password: passw0rd");

    auto [mock_network_manager_factory, guard] = mpt::MockNetworkManagerFactory::inject();
    auto mock_network_access_manager = std::make_unique<NiceMock<mpt::MockQNetworkAccessManager>>();
    auto mock_reply = new mpt::MockQNetworkReply();

    EXPECT_CALL(*mock_network_manager_factory, make_network_manager)
        .WillOnce([&mock_network_access_manager](auto...) {
            return std::move(mock_network_access_manager);
        });

    EXPECT_CALL(*mock_network_access_manager, createRequest).WillOnce(Return(mock_reply));
    EXPECT_CALL(*mock_reply, readData)
        .WillOnce([&fake_downloaded_yaml](char* data, auto) {
            auto data_size = fake_downloaded_yaml.size();
            memcpy(data, fake_downloaded_yaml.constData(), data_size);
            return data_size;
        })
        .WillOnce(Return(0));

    QTimer::singleShot(0, [&mock_reply] { mock_reply->finished(); });
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_EQ(send_command({"launch", "--cloud-init", fake_url.toStdString()}), mp::ReturnCode::Ok);
}

typedef typename std::tuple<mp::id_mappings, mp::id_mappings, mp::id_mappings, mp::id_mappings>
    id_test_tuples;
struct MountIdMappingsTest : public Client, public WithParamInterface<id_test_tuples>
{
};

TEST_P(MountIdMappingsTest, mountCmdIdMappings)
{
    const QTemporaryDir fake_directory{};
    const auto fake_source = fake_directory.path().toStdString();
    const auto fake_target = "instance_name:mounted_folder";

    auto [cmdline_uid_mappings,
          cmdline_gid_mappings,
          expected_uid_mappings,
          expected_gid_mappings] = GetParam();

    const auto id_mappings_matcher =
        make_mount_id_mappings_matcher(expected_uid_mappings, expected_gid_mappings);

    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(id_mappings_matcher, ok)));

    std::vector<std::string> cmdline{"mount", fake_source, fake_target};

    for (const auto& uid_element : cmdline_uid_mappings)
    {
        cmdline.push_back("--uid-map");
        cmdline.push_back(std::to_string(uid_element.first) + ":" +
                          std::to_string(uid_element.second));
    }

    for (const auto& gid_element : cmdline_gid_mappings)
    {
        cmdline.push_back("--gid-map");
        cmdline.push_back(std::to_string(gid_element.first) + ":" +
                          std::to_string(gid_element.second));
    }

    EXPECT_EQ(send_command(cmdline), mp::ReturnCode::Ok);
}

INSTANTIATE_TEST_SUITE_P(
    Client,
    MountIdMappingsTest,
    Values(id_test_tuples{{{500, 600}, {300, 400}},
                          {{700, 800}},
                          {{500, 600}, {300, 400}},
                          {{700, 800}}},
           id_test_tuples{{}, {{200, 300}}, {{mcp::getuid(), mp::default_id}}, {{200, 300}}},
           id_test_tuples{{{100, 200}}, {}, {{100, 200}}, {{mcp::getgid(), mp::default_id}}},
           id_test_tuples{{},
                          {},
                          {{mcp::getuid(), mp::default_id}},
                          {{mcp::getgid(), mp::default_id}}}));

struct TestInvalidNetworkOptions : Client, WithParamInterface<std::vector<std::string>>
{
};

TEST_P(TestInvalidNetworkOptions, launchCmdReturn)
{
    auto commands = GetParam();
    commands.insert(commands.begin(), std::string{"launch"});

    EXPECT_CALL(mock_daemon, launch(_, _)).Times(0);

    EXPECT_THAT(send_command(commands), Eq(mp::ReturnCode::CommandLineError));
}

INSTANTIATE_TEST_SUITE_P(Client,
                         TestInvalidNetworkOptions,
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

TEST_P(TestValidNetworkOptions, launchCmdReturn)
{
    auto commands = GetParam();
    commands.insert(commands.begin(), std::string{"launch"});

    EXPECT_CALL(mock_daemon, launch(_, _));

    EXPECT_THAT(send_command(commands), Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(
    Client,
    TestValidNetworkOptions,
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
TEST_F(Client, purgeCmdOkNoArgs)
{
    EXPECT_CALL(mock_daemon, purge(_, _));
    EXPECT_THAT(send_command({"purge"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, purgeCmdFailsWithArgs)
{
    EXPECT_THAT(send_command({"purge", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, purgeCmdHelpOk)
{
    EXPECT_THAT(send_command({"purge", "-h"}), Eq(mp::ReturnCode::Ok));
}

// exec cli tests
TEST_F(Client, execCmdDoubleDashOkCmdArg)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _));
    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());
    EXPECT_THAT(send_command({"exec", "foo", "--", "cmd"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, execCmdDoubleDashOkCmdArgWithOpts)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _));
    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());
    EXPECT_THAT(send_command({"exec", "foo", "--", "cmd", "--foo", "--bar"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, execCmdDoubleDashFailsMissingCmdArg)
{
    EXPECT_THAT(send_command({"exec", "foo", "--"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, execCmdNoDoubleDashOkCmdArg)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _));
    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());
    EXPECT_THAT(send_command({"exec", "foo", "cmd"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, execCmdNoDoubleDashOkMultipleArgs)
{
    EXPECT_CALL(mock_daemon, ssh_info(_, _));
    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, execCmdNoDoubleDashFailsCmdArgWithOpts)
{
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "--foo"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, execCmdHelpOk)
{
    EXPECT_THAT(send_command({"exec", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, execCmdNoDoubleDashUnknownOptionFailsPrintSuggestedCommand)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "--unknownOption"}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(cerr_stream.str(),
                HasSubstr("Options to the inner command should come after \"--\", like "
                          "this:\nmultipass exec <instance> -- "
                          "<command> <arguments>\n"));
}

TEST_F(Client, execCmdDoubleDashUnknownOptionFailsDoesNotPrintSuggestedCommand)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(
        send_command({"exec", "foo", "--unknownOption", "--", "cmd"}, trash_stream, cerr_stream),
        Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(cerr_stream.str(),
                Not(HasSubstr("Options to the inner command should come after \"--\", like "
                              "this:\nmultipass exec <instance> -- "
                              "<command> <arguments>\n")));
}

TEST_F(Client, execCmdNoDoubleDashNoUnknownOptionFailsDoesNotPrintSuggestedCommand)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(send_command({"exec", "foo", "cmd", "--help"}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cerr_stream.str(),
                Not(HasSubstr("Options to the inner command should come after \"--\", like "
                              "this:\nmultipass exec <instance> -- "
                              "<command> <arguments>\n")));
}

TEST_F(Client, execCmdStartsInstanceIfStoppedOrSuspended)
{
    const auto instance = "ordinary";
    const auto ssh_info_matcher = make_ssh_info_instance_matcher(instance);
    const auto start_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(instance);
    const grpc::Status aborted{grpc::StatusCode::ABORTED, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(ssh_info_matcher,
                                                                           aborted)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_matcher, ok)));
    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(ssh_info_matcher, ok)));

    EXPECT_THAT(send_command({"exec", instance, "--no-map-working-directory", "--", "command"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, execCmdFailsOnOtherAbsentInstance)
{
    const auto instance = "ordinary";
    const auto instance_matcher = make_ssh_info_instance_matcher(instance);
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, ssh_info)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SSHInfoReply, mp::SSHInfoRequest>(instance_matcher,
                                                                           notfound)));
    EXPECT_THAT(send_command({"exec", instance, "--no-map-working-directory", "--", "command"}),
                Eq(mp::ReturnCode::CommandFail));
}

struct SSHClientReturnTest : Client, WithParamInterface<int>
{
};

TEST_P(SSHClientReturnTest, execCmdWithoutDirWorks)
{
    const int failure_code{GetParam()};

    REPLACE(ssh_channel_get_exit_status, [&failure_code](auto) { return failure_code; });

    std::string instance_name{"instance"};
    mp::SSHInfoReply response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce(
            [&response](grpc::ServerContext* context,
                        grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
                server->Write(response);
                return grpc::Status{};
            });

    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());

    EXPECT_EQ(send_command({"exec", instance_name, "--", "cmd"}), failure_code);
}

TEST_P(SSHClientReturnTest, execCmdWithDirWorks)
{
    const int failure_code{GetParam()};

    REPLACE(ssh_channel_get_exit_status, [&failure_code](auto) { return failure_code; });

    std::string instance_name{"instance"};
    mp::SSHInfoReply response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce(
            [&response](grpc::ServerContext* context,
                        grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
                server->Write(response);
                return grpc::Status{};
            });

    EXPECT_EQ(
        send_command({"exec", instance_name, "--working-directory", "/home/ubuntu/", "--", "cmd"}),
        failure_code);
}

INSTANTIATE_TEST_SUITE_P(Client, SSHClientReturnTest, Values(0, -1, 1, 127));

TEST_F(Client, execCmdWithDirPrependsCd)
{
    std::string dir{"/home/ubuntu/"};
    std::string cmd{"pwd"};

    REPLACE(ssh_channel_request_exec, ([&dir, &cmd](ssh_channel, const char* raw_cmd) {
                EXPECT_THAT(raw_cmd, StartsWith("cd " + dir));
                EXPECT_THAT(raw_cmd, HasSubstr("&&"));
                EXPECT_THAT(raw_cmd, EndsWith(cmd)); // This will fail if cmd needs to be escaped.

                return SSH_OK;
            }));

    std::string instance_name{"instance"};
    mp::SSHInfoReply response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce(
            [&response](grpc::ServerContext* context,
                        grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
                server->Write(response);
                return grpc::Status{};
            });

    EXPECT_EQ(send_command({"exec", instance_name, "--working-directory", dir, "--", cmd}),
              mp::ReturnCode::Ok);
}

TEST_F(Client, execCmdWithDirAndSudoUsesSh)
{
    std::string dir{"/root/"};
    std::vector<std::string> cmds{"sudo", "pwd"};

    std::string cmds_string{cmds[0]};
    for (size_t i = 1; i < cmds.size(); ++i)
        cmds_string += " " + cmds[i];

    REPLACE(ssh_channel_request_exec, ([&dir, &cmds_string](ssh_channel, const char* raw_cmd) {
                // The test expects this exact command format
                // The issue is that when using sudo -u user, the AppArmor context is not preserved
                // But we need to match the actual implementation in exec.cpp
                EXPECT_EQ(raw_cmd,
                          "sudo sh -c cd\\ " + dir + "\\ \\&\\&\\ sudo\\ -u\\ user\\ " +
                              mpu::escape_for_shell(cmds_string));

                return SSH_OK;
            }));

    std::string instance_name{"instance"};
    mp::SSHInfoReply response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce(
            [&response](grpc::ServerContext* context,
                        grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
                server->Write(response);
                return grpc::Status{};
            });

    std::vector<std::string> full_cmdline{"exec", instance_name, "--working-directory", dir, "--"};
    for (const auto& c : cmds)
        full_cmdline.push_back(c);

    EXPECT_EQ(send_command(full_cmdline), mp::ReturnCode::Ok);
}

TEST_F(Client, execCmdFailsIfSshExecThrows)
{
    std::string dir{"/home/ubuntu/"};
    std::string cmd{"pwd"};

    REPLACE(ssh_channel_request_exec, ([](ssh_channel, const char* raw_cmd) {
                throw mp::SSHException("some exception");
                return SSH_OK;
            }));

    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::string instance_name{"instance"};
    mp::SSHInfoReply response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce(
            [&response](grpc::ServerContext* context,
                        grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
                server->Write(response);
                return grpc::Status{};
            });

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"exec", instance_name, "--", cmd}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandFail);
    EXPECT_THAT(cerr_stream.str(), HasSubstr("exec failed: some exception\n"));
}

TEST_F(Client, execFailsOnArgumentClash)
{
    std::stringstream cerr_stream;

    EXPECT_THAT(send_command({"exec",
                              "instance",
                              "--working-directory",
                              "/home/ubuntu/",
                              "--no-map-working-directory",
                              "--",
                              "cmd"},
                             trash_stream,
                             cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));

    EXPECT_THAT(cerr_stream.str(),
                Eq("Options --working-directory and --no-map-working-directory clash\n"));
}

// help cli tests
TEST_F(Client, helpCmdOkWithValidSingleArg)
{
    EXPECT_THAT(send_command({"help", "launch"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, helpCmdOkNoArgs)
{
    EXPECT_THAT(send_command({"help"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, helpCmdFailsWithInvalidArg)
{
    EXPECT_THAT(send_command({"help", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, helpCmdHelpOk)
{
    EXPECT_THAT(send_command({"help", "-h"}), Eq(mp::ReturnCode::Ok));
}

// info cli tests
TEST_F(Client, infoCmdOkNoArgs)
{
    EXPECT_CALL(mock_daemon, info(_, _));
    EXPECT_THAT(send_command({"info"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, info(_, _));
    EXPECT_THAT(send_command({"info", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, info(_, _));
    EXPECT_THAT(send_command({"info", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdHelpOk)
{
    EXPECT_THAT(send_command({"info", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, info(_, _));
    EXPECT_THAT(send_command({"info", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"info", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, infoCmdDoesNotDefaultToNoRuntimeInformationAndSucceeds)
{
    const auto info_matcher = Property(&mp::InfoRequest::no_runtime_information, IsFalse());

    EXPECT_CALL(mock_daemon, info)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::InfoReply, mp::InfoRequest>(info_matcher, ok)));
    EXPECT_THAT(send_command({"info", "name1", "name2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdSucceedsWithInstanceNamesAndNoRuntimeInformation)
{
    const auto info_matcher = Property(&mp::InfoRequest::no_runtime_information, IsTrue());

    EXPECT_CALL(mock_daemon, info)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::InfoReply, mp::InfoRequest>(info_matcher, ok)));
    EXPECT_THAT(send_command({"info", "name3", "name4", "--no-runtime-information"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, infoCmdSucceedsWithAllAndNoRuntimeInformation)
{
    const auto info_matcher = Property(&mp::InfoRequest::no_runtime_information, IsTrue());

    EXPECT_CALL(mock_daemon, info)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::InfoReply, mp::InfoRequest>(info_matcher, ok)));
    EXPECT_THAT(send_command({"info", "name5", "--no-runtime-information"}),
                Eq(mp::ReturnCode::Ok));
}

// list cli tests
TEST_F(Client, listCmdOkNoArgs)
{
    const auto list_matcher = Property(&mp::ListRequest::request_ipv4, IsTrue());
    mp::ListReply reply;
    reply.mutable_instance_list();

    EXPECT_CALL(mock_daemon, list)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::ListReply, mp::ListRequest>(list_matcher, ok, reply)));
    EXPECT_THAT(send_command({"list"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, listCmdFailsWithArgs)
{
    EXPECT_THAT(send_command({"list", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, listCmdHelpOk)
{
    EXPECT_THAT(send_command({"list", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, listCmdNoIpv4Ok)
{
    const auto list_matcher = Property(&mp::ListRequest::request_ipv4, IsFalse());
    mp::ListReply reply;
    reply.mutable_instance_list();

    EXPECT_CALL(mock_daemon, list)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::ListReply, mp::ListRequest>(list_matcher, ok, reply)));
    EXPECT_THAT(send_command({"list", "--no-ipv4"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, listCmdFailsWithIpv4AndSnapshots)
{
    EXPECT_THAT(send_command({"list", "--no-ipv4", "--snapshots"}),
                Eq(mp::ReturnCode::CommandLineError));
}

// mount cli tests
// Note: mpt::test_data_path() returns an absolute path
TEST_F(Client, mountCmdGoodAbsoluteSourcePath)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_THAT(send_command({"mount", mpt::test_data_path().toStdString(), "test-vm:test"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mountCmdGoodRelativeSourcePath)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_THAT(send_command({"mount", "..", "test-vm:test"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mountCmdFailsInvalidSourcePath)
{
    EXPECT_THAT(
        send_command({"mount", mpt::test_data_path_for("foo").toStdString(), "test-vm:test"}),
        Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mountCmdGoodValidUidMappings)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-u", "1000:501", "test-vm:test"}),
        Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mountCmdGoodValidLargeUidMappings)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-u", "218038053:0", "test-vm:test"}),
        Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mountCmdFailsInvalidStringUidMappings)
{
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-u", "foo:bar", "test-vm:test"}),
        Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mountCmdFailsInvalidHostIntUidMappings)
{
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-u", "5000000000:0", "test-vm:test"}),
        Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mountCmdGoodValidGidMappings)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-g", "1000:501", "test-vm:test"}),
        Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mountCmdGoodValidLargeGidMappings)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-g", "218038053:0", "test-vm:test"}),
        Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, mountCmdFailsInvalidStringGidMappings)
{
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-g", "foo:bar", "test-vm:test"}),
        Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mountCmdFailsInvalidHostIntGidMappings)
{
    EXPECT_THAT(
        send_command(
            {"mount", mpt::test_data_path().toStdString(), "-g", "5000000000:0", "test-vm:test"}),
        Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, mountCmdGoodClassicMountType)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_EQ(send_command(
                  {"mount", "-t", "classic", mpt::test_data_path().toStdString(), "test-vm:test"}),
              mp::ReturnCode::Ok);
}

TEST_F(Client, mountCmdGoodNativeMountType)
{
    EXPECT_CALL(mock_daemon, mount(_, _));
    EXPECT_EQ(send_command(
                  {"mount", "-t", "native", mpt::test_data_path().toStdString(), "test-vm:test"}),
              mp::ReturnCode::Ok);
}

TEST_F(Client, mountCmdFailsBogusMountType)
{
    EXPECT_EQ(
        send_command({"mount", "-t", "bogus", mpt::test_data_path().toStdString(), "test-vm:test"}),
        mp::ReturnCode::CommandLineError);
}

// recover cli tests
TEST_F(Client, recoverCmdFailsNoArgs)
{
    EXPECT_THAT(send_command({"recover"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, recoverCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, recover(_, _));
    EXPECT_THAT(send_command({"recover", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recoverCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, recover(_, _));
    EXPECT_THAT(send_command({"recover", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recoverCmdHelpOk)
{
    EXPECT_THAT(send_command({"recover", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recoverCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, recover(_, _));
    EXPECT_THAT(send_command({"recover", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, recoverCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"recover", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

// start cli tests
TEST_F(Client, startCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, start(_, _));
    EXPECT_THAT(send_command({"start", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, start(_, _));
    EXPECT_THAT(send_command({"start", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdHelpOk)
{
    EXPECT_THAT(send_command({"start", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, start(_, _));
    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"start", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, startCmdNoArgsTargetsPetenv)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdConsidersConfiguredPetenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdCanTargetPetenvExplicitly)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"start", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdCanTargetPetenvAmongOthers)
{
    const auto petenv_matcher2 =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 2>(petenv_name);
    const auto petenv_matcher4 =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 4>(petenv_name);

    InSequence s;
    EXPECT_CALL(mock_daemon, start(_, _));
    EXPECT_CALL(mock_daemon, start)
        .Times(2)
        .WillRepeatedly(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_matcher2, ok)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_matcher4, ok)));
    EXPECT_THAT(send_command({"start", "primary"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", "foo", petenv_name}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", petenv_name, "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start", "foo", petenv_name, "bar", "baz"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdDisabledPetenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, start(_, _));

    EXPECT_THAT(send_command({"start", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"start"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, startCmdDisabledPetenvAll)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, start(_, _));

    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdDisabledPetenvHelp)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, start(_, _)).Times(0);

    EXPECT_THAT(send_command({"start", "-h"}), Eq(mp::ReturnCode::Ok));
}

// version cli tests
TEST_F(Client, versionWithoutArg)
{
    EXPECT_CALL(mock_daemon, version(_, _));
    EXPECT_THAT(send_command({"version"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, versionWithPositionalFormatArg)
{
    EXPECT_THAT(send_command({"version", "format"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, versionWithOptionFormatArg)
{
    EXPECT_CALL(mock_daemon, version(_, _)).Times(4);
    EXPECT_THAT(send_command({"version", "--format=table"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"version", "--format=yaml"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"version", "--format=json"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"version", "--format=csv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, versionWithOptionFormatInvalidArg)
{
    EXPECT_THAT(send_command({"version", "--format=default"}),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"version", "--format=MumboJumbo"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, versionParseFailure)
{
    EXPECT_THAT(send_command({"version", "--format"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, versionInfoOnFailure)
{
    const grpc::Status notfound{grpc::StatusCode::NOT_FOUND, "msg"};

    EXPECT_CALL(mock_daemon, version(_, _)).WillOnce(Return(notfound));
    EXPECT_THAT(send_command({"version", "--format=yaml"}), Eq(mp::ReturnCode::Ok));
}

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

std::vector<std::string> concat(const std::vector<std::string>& v1,
                                const std::vector<std::string>& v2)
{
    auto ret = v1;
    ret.insert(end(ret), cbegin(v2), cend(v2));

    return ret;
}

TEST_F(Client, startCmdLaunchesPetenvIfAbsent)
{
    const auto petenv_start_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name);
    const grpc::Status aborted = aborted_start_status({petenv_name});

    EXPECT_CALL(mock_daemon, mount).WillRepeatedly(Return(ok)); // 0 or more times

    InSequence seq;
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_start_matcher,
                                                                       aborted)));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(petenv_launch_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_start_matcher, ok)));
    EXPECT_THAT(send_command({"start", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdAutomountsWhenLaunchingPetenv)
{
    const grpc::Status aborted = aborted_start_status({petenv_name});

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start(_, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdSkipsAutomountWhenDisabled)
{
    std::stringstream cout_stream;
    const grpc::Status aborted = aborted_start_status({petenv_name});

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillOnce(Return("false"));
    EXPECT_CALL(mock_daemon, mount(_, _)).Times(0);
    EXPECT_CALL(mock_daemon, start(_, _)).WillOnce(Return(ok));
    EXPECT_THAT(send_command({"start", petenv_name}, cout_stream), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping mount due to disabled mounts feature\n"));
}

TEST_F(Client, startCmdForwardsVerbosityToSubcommands)
{
    const grpc::Status aborted = aborted_start_status({petenv_name});
    const auto verbosity = 2;
    const auto start_verbosity_matcher =
        make_request_verbosity_matcher<mp::StartRequest>(verbosity);
    const auto launch_verbosity_matcher =
        make_request_verbosity_matcher<mp::LaunchRequest>(verbosity);
    const auto mount_verbosity_matcher =
        make_request_verbosity_matcher<mp::MountRequest>(verbosity);

    InSequence seq;
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_verbosity_matcher,
                                                                       aborted)));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_verbosity_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::MountReply, mp::MountRequest>(mount_verbosity_matcher,
                                                                       ok)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_verbosity_matcher,
                                                                       ok)));
    EXPECT_THAT(send_command({"start", "-vv"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdForwardsTimeoutToSubcommands)
{
    const grpc::Status aborted = aborted_start_status({petenv_name});
    const auto timeout = 123;
    const auto start_timeout_matcher = make_request_timeout_matcher<mp::StartRequest>(timeout);
    const auto launch_timeout_matcher = make_request_timeout_matcher<mp::LaunchRequest>(timeout);

    InSequence seq;
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_timeout_matcher,
                                                                       aborted)));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(launch_timeout_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, mount).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(start_timeout_matcher, ok)));
    EXPECT_THAT(send_command({"start", "--timeout", std::to_string(timeout)}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const auto aborted = aborted_start_status({petenv_name});
    const auto except =
        mp::RemoteHandlerException{grpc::Status{grpc::StatusCode::INTERNAL, "oops"}};

    InSequence seq;
    EXPECT_CALL(mock_daemon, start).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillOnce(Throw(except));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"start", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdFailsWhenAutomountingInPetenvFails)
{
    const auto aborted = aborted_start_status({petenv_name});
    const auto mount_failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, start(_, _)).WillOnce(Return(aborted));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"start", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdLaunchesPetenvIfAbsentAmongOthersPresent)
{
    std::vector<std::string> instances{"a", "b", petenv_name, "c"},
        cmd = concat({"start"}, instances);

    const auto instance_start_matcher =
        make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name);
    const grpc::Status aborted = aborted_start_status({petenv_name});

    EXPECT_CALL(mock_daemon, mount).WillRepeatedly(Return(ok)); // 0 or more times

    InSequence seq;
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       aborted)));
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::LaunchReply, mp::LaunchRequest>(petenv_launch_matcher,
                                                                         ok)));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       ok)));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdFailsIfPetenvIfAbsentAmountOthersAbsent)
{
    std::vector<std::string> instances{"a", "b", "c", petenv_name, "xyz"},
        cmd = concat({"start"}, instances);

    const auto instance_start_matcher =
        make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted =
        aborted_start_status({std::next(std::cbegin(instances), 2), std::cend(instances)});

    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       aborted)));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdFailsIfPetenvIfAbsentAmountOthersDeleted)
{
    std::vector<std::string> instances{"nope", petenv_name}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher =
        make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {instances.front()});

    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       aborted)));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdFailsIfPetenvPresentButDeleted)
{
    const auto petenv_start_matcher =
        make_instance_in_repeated_field_matcher<mp::StartRequest, 1>(petenv_name);
    const grpc::Status aborted = aborted_start_status({}, {petenv_name});

    InSequence seq;
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(petenv_start_matcher,
                                                                       aborted)));
    EXPECT_THAT(send_command({"start", petenv_name}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdFailsIfPetenvPresentButDeletedAmongOthers)
{
    std::vector<std::string> instances{petenv_name, "other"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher =
        make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {instances.front()});

    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       aborted)));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdFailsOnOtherAbsentInstance)
{
    std::vector<std::string> instances{"o-o", "O_o"}, cmd = concat({"start"}, instances);

    const auto instance_start_matcher =
        make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {"O_o"});

    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       aborted)));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdFailsOnOtherAbsentInstancesWithPetenv)
{
    std::vector<std::string> cmd{"start"}, instances{petenv_name, "lala", "zzz"};
    cmd.insert(end(cmd), cbegin(instances), cend(instances));

    const auto instance_start_matcher =
        make_instances_sequence_matcher<mp::StartRequest>(instances);
    const auto aborted = aborted_start_status({}, {"zzz"});
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StartReply, mp::StartRequest>(instance_start_matcher,
                                                                       aborted)));
    EXPECT_THAT(send_command(cmd), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, startCmdDoesNotAddPetenvToOthers)
{
    const auto matcher =
        make_instances_matcher<mp::StartRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::StartReply, mp::StartRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"start", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, startCmdDoesNotAddPetenvToAll)
{
    const auto matcher = make_instances_matcher<mp::StartRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, start)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::StartReply, mp::StartRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"start", "--all"}), Eq(mp::ReturnCode::Ok));
}

// stop cli tests
TEST_F(Client, stopCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_THAT(send_command({"stop", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_THAT(send_command({"stop", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdHelpOk)
{
    EXPECT_THAT(send_command({"stop", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"stop", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdNoArgsTargetsPetenv)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StopReply, mp::StopRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdConsidersConfiguredPetenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StopReply, mp::StopRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdCanTargetPetenvExplicitly)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StopReply, mp::StopRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"stop", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdCanTargetPetenvAmongOthers)
{
    const auto petenv_matcher2 =
        make_instance_in_repeated_field_matcher<mp::StopRequest, 2>(petenv_name);
    const auto petenv_matcher4 =
        make_instance_in_repeated_field_matcher<mp::StopRequest, 4>(petenv_name);

    InSequence s;
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_CALL(mock_daemon, stop)
        .Times(2)
        .WillRepeatedly(WithArg<1>(
            check_request_and_return<mp::StopReply, mp::StopRequest>(petenv_matcher2, ok)));
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StopReply, mp::StopRequest>(petenv_matcher4, ok)));
    EXPECT_THAT(send_command({"stop", "primary"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", "foo", petenv_name}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", petenv_name, "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", "foo", petenv_name, "bar", "baz"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdDoesNotAddPetenvToOthers)
{
    const auto matcher =
        make_instances_matcher<mp::StopRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::StopReply, mp::StopRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"stop", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdDoesNotAddPetenvToAll)
{
    const auto matcher = make_instances_matcher<mp::StopRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::StopReply, mp::StopRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdFailsWithTimeAndCancel)
{
    EXPECT_THAT(send_command({"stop", "--time", "+10", "--cancel", "foo"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdSucceedsWithPlusTime)
{
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+10"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdSucceedsWithNoPlusTime)
{
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_THAT(send_command({"stop", "foo", "--time", "10"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdFailsWithInvalidTimePrefix)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "-10"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdFailsWithInvalidTime)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdFailsWithTimeSuffix)
{
    EXPECT_THAT(send_command({"stop", "foo", "--time", "+10s"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdSucceedsWithCancel)
{
    EXPECT_CALL(mock_daemon, stop(_, _));
    EXPECT_THAT(send_command({"stop", "foo", "--cancel"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdNoArgsTimeOptionDelaysPetenvShutdown)
{
    const auto delay = 5;
    const auto matcher =
        AllOf(make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name),
              Property(&mp::StopRequest::time_minutes, delay));
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::StopReply, mp::StopRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"stop", "--time", std::to_string(delay)}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdNoArgsCancelOptionCancelsDelayedPetenvShutdown)
{
    const auto matcher =
        AllOf(make_instance_in_repeated_field_matcher<mp::StopRequest, 1>(petenv_name),
              Property(&mp::StopRequest::cancel_shutdown, true));
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::StopReply, mp::StopRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"stop", "--cancel"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdNoArgsFailsWithTimeAndCancel)
{
    EXPECT_THAT(send_command({"stop", "--time", "+10", "--cancel"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdDisabledPetenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "--cancel"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "--time", "10"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdDisabledPetenvWithInstance)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, stop(_, _));

    EXPECT_THAT(send_command({"stop"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"stop", "--cancel"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"stop", "--time", "10"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdDisabledPetenvHelp)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"stop", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdDisabledPetenvAll)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, stop(_, _));

    EXPECT_THAT(send_command({"stop", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdForceSendsProperRequest)
{
    const auto force_set_matcher = Property(&mp::StopRequest::force_stop, IsTrue());
    EXPECT_CALL(mock_daemon, stop)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::StopReply, mp::StopRequest>(force_set_matcher, ok)));

    EXPECT_THAT(send_command({"stop", "foo", "--force"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, stopCmdForceConflictsWithTimeOption)
{
    EXPECT_THAT(send_command({"stop", "foo", "--force", "--time", "10"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdForceConflictsWithCancelOption)
{
    EXPECT_THAT(send_command({"stop", "foo", "--force", "--cancel"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, stopCmdWrongVmState)
{
    const auto invalid_vm_state_failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
    EXPECT_CALL(mock_daemon, stop(_, _)).WillOnce(Return(invalid_vm_state_failure));
    EXPECT_THAT(send_command({"stop", "foo"}), Eq(mp::ReturnCode::CommandFail));
}

// suspend cli tests
TEST_F(Client, suspendCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, suspend(_, _)).Times(2);
    EXPECT_THAT(send_command({"suspend", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"suspend", "primary"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, suspend(_, _));
    EXPECT_THAT(send_command({"suspend", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdHelpOk)
{
    EXPECT_THAT(send_command({"suspend", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, suspend(_, _));
    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdNoArgsTargetsPetenv)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::SuspendRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, suspend)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdConsidersConfiguredPetenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::SuspendRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, suspend)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdCanTargetPetenvExplicitly)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::SuspendRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, suspend)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"suspend", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdCanTargetPetenvAmongOthers)
{
    const auto petenv_matcher2 =
        make_instance_in_repeated_field_matcher<mp::SuspendRequest, 2>(petenv_name);
    const auto petenv_matcher4 =
        make_instance_in_repeated_field_matcher<mp::SuspendRequest, 4>(petenv_name);

    InSequence s;
    EXPECT_CALL(mock_daemon, suspend)
        .Times(2)
        .WillRepeatedly(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(petenv_matcher2, ok)));
    EXPECT_CALL(mock_daemon, suspend)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(petenv_matcher4, ok)));
    EXPECT_THAT(send_command({"suspend", "foo", petenv_name}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"suspend", petenv_name, "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"suspend", "foo", petenv_name, "bar", "baz"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdDoesNotAddPetenvToOthers)
{
    const auto matcher =
        make_instances_matcher<mp::SuspendRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, suspend)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"suspend", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdDoesNotAddPetenvToAll)
{
    const auto matcher = make_instances_matcher<mp::SuspendRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, suspend)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::SuspendReply, mp::SuspendRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"suspend", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, suspendCmdDisabledPetenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, suspend(_, _));

    EXPECT_THAT(send_command({"suspend"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"suspend", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdDisabledPetenvHelp)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"suspend", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, suspendCmdDisabledPetenvAll)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, suspend(_, _));

    EXPECT_THAT(send_command({"suspend", "--all"}), Eq(mp::ReturnCode::Ok));
}

// restart cli tests
TEST_F(Client, restartCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, restart(_, _)).Times(2);
    EXPECT_THAT(send_command({"restart", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"restart", "primary"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, restart(_, _));
    EXPECT_THAT(send_command({"restart", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdHelpOk)
{
    EXPECT_THAT(send_command({"restart", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, restart(_, _));
    EXPECT_THAT(send_command({"restart", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdNoArgsTargetsPetenv)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdConsidersConfiguredPetenv)
{
    const auto custom_petenv = "jarjar binks";
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(custom_petenv);
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdCanTargetPetenvExplicitly)
{
    const auto petenv_matcher =
        make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(petenv_name);
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(petenv_matcher, ok)));
    EXPECT_THAT(send_command({"restart", petenv_name}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdCanTargetPetenvAmongOthers)
{
    const auto petenv_matcher2 =
        make_instance_in_repeated_field_matcher<mp::RestartRequest, 2>(petenv_name);
    const auto petenv_matcher4 =
        make_instance_in_repeated_field_matcher<mp::RestartRequest, 4>(petenv_name);

    InSequence s;
    EXPECT_CALL(mock_daemon, restart)
        .Times(2)
        .WillRepeatedly(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(petenv_matcher2, ok)));
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(petenv_matcher4, ok)));
    EXPECT_THAT(send_command({"restart", "foo", petenv_name}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"restart", petenv_name, "bar"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"restart", "foo", petenv_name, "bar", "baz"}),
                Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdDoesNotAddPetenvToOthers)
{
    const auto matcher =
        make_instances_matcher<mp::RestartRequest>(ElementsAre(StrEq("foo"), StrEq("bar")));
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"restart", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdDoesNotAddPetenvToAll)
{
    const auto matcher = make_instances_matcher<mp::RestartRequest>(IsEmpty());
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(matcher, ok)));
    EXPECT_THAT(send_command({"restart", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"restart", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, restartCmdFailsWithUnknownOptions)
{
    EXPECT_THAT(send_command({"restart", "-x", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-wrong", "--all"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-h", "--nope", "not"}),
                Eq(mp::ReturnCode::CommandLineError));

    // Options that would be accepted by stop
    EXPECT_THAT(send_command({"restart", "-t", "foo"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-t0", "bar"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "--time", "42", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "-c", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "--cancel", "foo"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, restartCmdFailsIfPetenvNonexistent)
{
    const auto petenv_start_matcher =
        make_instance_in_repeated_field_matcher<mp::RestartRequest, 1>(petenv_name);
    const grpc::Status aborted = aborted_start_status({}, {petenv_name});

    InSequence seq;
    EXPECT_CALL(mock_daemon, restart)
        .WillOnce(WithArg<1>(
            check_request_and_return<mp::RestartReply, mp::RestartRequest>(petenv_start_matcher,
                                                                           aborted)));
    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, restartCmdDisabledPetenv)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, restart(_, _));

    EXPECT_THAT(send_command({"restart"}), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(send_command({"restart", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdDisabledPetenvHelp)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));

    EXPECT_THAT(send_command({"restart", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, restartCmdDisabledPetenvAll)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return(""));
    EXPECT_CALL(mock_daemon, restart(_, _));

    EXPECT_THAT(send_command({"restart", "--all"}), Eq(mp::ReturnCode::Ok));
}

// delete cli tests
TEST_F(Client, deleteCmdFailsNoArgs)
{
    EXPECT_THAT(send_command({"delete"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, deleteCmdOkWithOneArg)
{
    EXPECT_CALL(mock_daemon, delet(_, _));
    EXPECT_THAT(send_command({"delete", "foo"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, deleteCmdSucceedsWithMultipleArgs)
{
    EXPECT_CALL(mock_daemon, delet(_, _));
    EXPECT_THAT(send_command({"delete", "foo", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, deleteCmdHelpOk)
{
    EXPECT_THAT(send_command({"delete", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, deleteCmdSucceedsWithAll)
{
    EXPECT_CALL(mock_daemon, delet(_, _));
    EXPECT_THAT(send_command({"delete", "--all"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, deleteCmdFailsWithNamesAndAll)
{
    EXPECT_THAT(send_command({"delete", "--all", "foo", "bar"}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, deleteCmdAcceptsPurgeOption)
{
    EXPECT_CALL(mock_daemon, delet(_, _)).Times(2);
    EXPECT_THAT(send_command({"delete", "--purge", "foo"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"delete", "-p", "bar"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, deleteCmdWrongVmState)
{
    const auto invalid_vm_state_failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
    EXPECT_CALL(mock_daemon, delet(_, _)).WillOnce(Return(invalid_vm_state_failure));
    EXPECT_THAT(send_command({"delete", "foo"}), Eq(mp::ReturnCode::CommandFail));
}

// find cli tests
TEST_F(Client, findCmdUnsupportedOptionOk)
{
    EXPECT_CALL(mock_daemon, find(_, _));
    EXPECT_THAT(send_command({"find", "--show-unsupported"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, findCmdForceUpdateOk)
{
    EXPECT_CALL(mock_daemon, find(_, _));
    EXPECT_EQ(send_command({"find", "--force-update"}), mp::ReturnCode::Ok);
}

TEST_F(Client, findCmdForceUpdateWithRemoteOk)
{
    EXPECT_CALL(mock_daemon, find(_, _));
    EXPECT_EQ(send_command({"find", "foo:", "--force-update"}), mp::ReturnCode::Ok);
}

TEST_F(Client, findCmdForceUpdateWithRemoteAndSearchNameOk)
{
    EXPECT_CALL(mock_daemon, find(_, _));
    EXPECT_EQ(send_command({"find", "foo:bar", "--force-update"}), mp::ReturnCode::Ok);
}

TEST_F(Client, findCmdTooManyArgsFails)
{
    EXPECT_THAT(send_command({"find", "foo", "bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, findCmdMultipleColonsFails)
{
    EXPECT_THAT(send_command({"find", "foo::bar"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, findCmdHelpOk)
{
    EXPECT_THAT(send_command({"find", "-h"}), Eq(mp::ReturnCode::Ok));
}

// wait-ready cli tests
TEST_F(Client, waitReadyCmdNoArgsOk)
{
    EXPECT_CALL(mock_daemon, wait_ready(_, _)).WillOnce(Return(grpc::Status::OK));
    EXPECT_THAT(send_command({"wait-ready"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, waitReadyCmdHelpOk)
{
    EXPECT_THAT(send_command({"wait-ready", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, waitReadyCmdPollsDaemonOk)
{
    grpc::Status daemon_not_ready(grpc::StatusCode::NOT_FOUND,
                                  "cannot connect to the multipass socket");

    InSequence seq;
    EXPECT_CALL(mock_daemon, wait_ready(_, _)).WillOnce(Return(daemon_not_ready));
    EXPECT_CALL(*mock_utils, sleep_for).Times(1);
    EXPECT_CALL(mock_daemon, wait_ready(_, _)).WillOnce(Return(grpc::Status::OK));

    EXPECT_THAT(send_command({"wait-ready"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, waitReadyCmdPollsOnImageServerConnectionFailure)
{
    grpc::Status daemon_not_connected_to_image_servers(grpc::StatusCode::NOT_FOUND,
                                                       "cannot connect to the image servers");

    InSequence seq;
    EXPECT_CALL(mock_daemon, wait_ready(_, _))
        .WillOnce(Return(daemon_not_connected_to_image_servers));
    EXPECT_CALL(*mock_utils, sleep_for).Times(1);
    EXPECT_CALL(mock_daemon, wait_ready(_, _)).WillOnce(Return(grpc::Status::OK));

    EXPECT_THAT(send_command({"wait-ready"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, waitReadyCmdFailsWithUnknownArg)
{
    EXPECT_THAT(send_command({"wait-ready", "--unknown"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, waitReadyCmdFailsWhenDaemonFails)
{
    grpc::Status daemon_failure(grpc::StatusCode::FAILED_PRECONDITION, "");

    EXPECT_CALL(mock_daemon, wait_ready(_, _)).WillOnce(Return(daemon_failure));

    EXPECT_THAT(send_command({"wait-ready"}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, waitReadyCmdWithTimeoutFailsWhenDaemonFails)
{
    grpc::Status daemon_failure(grpc::StatusCode::FAILED_PRECONDITION, "");

    EXPECT_CALL(mock_daemon, wait_ready(_, _)).WillOnce(Return(daemon_failure));

    EXPECT_THAT(send_command({"wait-ready", "--timeout", "10"}), Eq(mp::ReturnCode::CommandFail));
}

// get/set cli tests
struct TestGetSetHelp : Client, WithParamInterface<std::string>
{
};

TEST_P(TestGetSetHelp, helpIncludesKeyExamplesAndHowToGetFullList)
{
    std::ostringstream cout;

    EXPECT_THAT(send_command({GetParam(), "--help"}, cout), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout.str(),
                AllOf(HasSubstr("local."), HasSubstr("client."), HasSubstr("get --keys")));
}

INSTANTIATE_TEST_SUITE_P(Client, TestGetSetHelp, Values("get", "set"));

struct TestBasicGetSetOptions : Client, WithParamInterface<const char*>
{
};

TEST_P(TestBasicGetSetOptions, getCanReadSettings)
{
    const auto& key = GetParam();
    const auto value = "a value";
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(value));
    EXPECT_THAT(get_setting(key), Eq(value));
}

TEST_P(TestBasicGetSetOptions, setCanWriteSettings)
{
    const auto& key = GetParam();
    const auto val = "blah";

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::Ok));
}

TEST_P(TestBasicGetSetOptions, setCmdAllowsEmptyVal)
{
    const auto& key = GetParam();
    const auto val = "";

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::Ok));
}

TEST_P(TestBasicGetSetOptions, interactiveSetWritesSettings)
{
    const auto& key = GetParam();
    const auto val = "blah";
    std::istringstream cin{fmt::format("{}\n", val)};

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)));
    EXPECT_THAT(send_command({"set", key}, trash_stream, trash_stream, cin),
                Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(Client,
                         TestBasicGetSetOptions,
                         Values(mp::petenv_key,
                                mp::driver_key,
                                mp::bridged_interface_key,
                                mp::mounts_key,
                                "anything.else.really"));

TEST_F(Client, getReturnsSetting)
{
    const auto key = "sigur", val = "ros";
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(val));
    EXPECT_THAT(get_setting(key), Eq(val));
}

TEST_F(Client, getCmdFailsWithNoArguments)
{
    EXPECT_THAT(send_command({"get"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, setCmdFailsWithNoArguments)
{
    EXPECT_CALL(mock_settings, set(_, _)).Times(0);
    EXPECT_THAT(send_command({"set"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, getCmdFailsWithMultipleArguments)
{
    EXPECT_THAT(send_command({"get", mp::petenv_key, mp::driver_key}),
                Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, setCmdFailsWithMultipleArguments)
{
    EXPECT_CALL(mock_settings, set(_, _)).Times(0);
    EXPECT_THAT(
        send_command(
            {"set", keyval_arg(mp::petenv_key, "asdf"), keyval_arg(mp::driver_key, "qemu")}),
        Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, setCmdFailsWithBadKeyValFormat)
{
    EXPECT_CALL(mock_settings, set(_, _)).Times(0); // this is not where the rejection is here
    EXPECT_THAT(send_command({"set", "="}), Eq(mp::ReturnCode::CommandLineError));
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

TEST_F(Client, interactiveSetFailsWithEOF)
{
    std::ostringstream cerr;
    std::istringstream cin;

    EXPECT_THAT(send_command({"set", mp::petenv_key}, trash_stream, cerr, cin),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(cerr.str(), HasSubstr("Failed to read value"));
}

TEST_F(Client, getCmdFailsWithUnknownKey)
{
    const auto key = "wrong.key";
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
    EXPECT_THAT(send_command({"get", key}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, setCmdFailsWithUnknownKey)
{
    const auto key = "wrong.key";
    const auto val = "blah";
    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
        .WillOnce(Throw(mp::UnrecognizedSettingException{key}));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, interactiveSetFailsWithUnknownKey)
{
    const auto key = "wrong.key";
    const auto val = "blah";
    std::ostringstream cerr;
    std::istringstream cin{fmt::format("{}\n", val)};

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
        .WillOnce(Throw(mp::UnrecognizedSettingException{key}));
    EXPECT_THAT(send_command({"set", key}, trash_stream, cerr, cin),
                Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(cerr.str(), HasSubstr("Unrecognized settings key: 'wrong.key'"));
}

TEST_F(Client, getHandlesPersistentSettingsErrors)
{
    const auto key = mp::petenv_key;
    EXPECT_CALL(mock_settings, get(Eq(key)))
        .WillOnce(Throw(mp::PersistentSettingsException{"op", "test"}));
    EXPECT_THAT(send_command({"get", key}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, getReturnsSpecialRepresentationOfEmptyValueByDefault)
{
    const auto key = mp::petenv_key;
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(QStringLiteral("")));
    EXPECT_THAT(get_setting(key), Eq("<empty>"));
}

TEST_F(Client, getReturnsEmptyStringOnEmptyValueWithRawOption)
{
    const auto key = mp::petenv_key;
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(QStringLiteral("")));
    EXPECT_THAT(get_setting({key, "--raw"}), IsEmpty());
}

TEST_F(Client, getKeepsOtherValuesUntouchedWithRawOption)
{
    std::vector<std::pair<const char*, QString>> keyvals{
        {mp::petenv_key, QStringLiteral("a-pet-nAmE")}};
    for (const auto& [key, val] : keyvals)
    {
        EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(val));
        EXPECT_THAT(get_setting({key, "--raw"}), Eq(val.toStdString()));
    }
}

TEST_F(Client, getKeysNoArgReturnsAllSettingsKeys)
{
    const auto key_set = std::set<QString>{"asdf", "sdfg", "dfgh"};
    EXPECT_CALL(mock_settings, keys).WillOnce(Return(key_set));

    auto got_keys = QString::fromStdString(get_setting("--keys")).split('\n');
    EXPECT_THAT(got_keys, UnorderedElementsAreArray(key_set));
}

TEST_F(Client, getKeysWithValidKeyReturnsThatKey)
{
    constexpr auto key = "foo";
    const auto key_set = std::set<QString>{"asdf", "sdfg", "dfgh", key};
    EXPECT_CALL(mock_settings, keys).WillOnce(Return(key_set));

    EXPECT_THAT(get_setting({"--keys", key}), StrEq(key));
}

TEST_F(Client, getKeysWithUnrecognizedKeyFails)
{
    constexpr auto wildcard = "*not*yet*";
    const auto key_set = std::set<QString>{"asdf", "sdfg", "dfgh"};
    EXPECT_CALL(mock_settings, keys).WillOnce(Return(key_set));

    std::ostringstream cout, cerr;
    EXPECT_THAT(send_command({"get", "--keys", wildcard}, cout, cerr),
                Eq(mp::ReturnCode::CommandLineError));

    EXPECT_THAT(cerr.str(), AllOf(HasSubstr("Unrecognized"), HasSubstr(wildcard)));
    EXPECT_THAT(cout.str(), IsEmpty());
}

TEST_F(Client, setHandlesPersistentSettingsErrors)
{
    const auto key = mp::petenv_key;
    const auto val = "asdasdasd";
    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
        .WillOnce(Throw(mp::PersistentSettingsException{"op", "test"}));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, setCmdRejectsBadValues)
{
    const auto key = "hip", val = "hop", why = "don't like it";
    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val)))
        .WillOnce(Throw(mp::InvalidSettingException{key, val, why}));
    EXPECT_THAT(send_command({"set", keyval_arg(key, val)}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, setCmdTogglePetenv)
{
    EXPECT_CALL(mock_settings, set(Eq(mp::petenv_key), Eq("")));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, "")}), Eq(mp::ReturnCode::Ok));

    EXPECT_CALL(mock_settings, set(Eq(mp::petenv_key), Eq("some primary")));
    EXPECT_THAT(send_command({"set", keyval_arg(mp::petenv_key, "some primary")}),
                Eq(mp::ReturnCode::Ok));
}

// general help tests
TEST_F(Client, helpReturnsOkReturnCode)
{
    EXPECT_THAT(send_command({"--help"}), Eq(mp::ReturnCode::Ok));
}

struct HelpTestsuite : public ClientAlias,
                       public WithParamInterface<std::pair<std::string, std::string>>
{
};

TEST_P(HelpTestsuite, answersCorrectly)
{
    auto [command, expected_text] = GetParam();

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"help", command}, cout_stream), mp::ReturnCode::Ok);
    EXPECT_THAT(cout_stream.str(), HasSubstr(expected_text));

    cout_stream.str(std::string());
    EXPECT_EQ(send_command({command, "-h"}, cout_stream), mp::ReturnCode::Ok);
    EXPECT_THAT(cout_stream.str(), HasSubstr(expected_text));
}

INSTANTIATE_TEST_SUITE_P(
    Client,
    HelpTestsuite,
    Values(std::make_pair(std::string{"alias"},
                          "Create an alias to be executed on a given instance.\n"),
           std::make_pair(std::string{"aliases"}, "List available aliases\n"),
           std::make_pair(std::string{"unalias"}, "Remove aliases\n"),
           std::make_pair(std::string{"prefer"},
                          "Switch the current alias context. If it does not exist, create it "
                          "before switching.")));

TEST_F(Client, commandHelpIsDifferentThanGeneralHelp)
{
    std::stringstream general_help_output;
    send_command({"--help"}, general_help_output);

    std::stringstream command_output;
    send_command({"list", "--help"}, command_output);

    EXPECT_THAT(general_help_output.str(), Ne(command_output.str()));
}

TEST_F(Client, helpCmdLaunchSameLaunchCmdHelp)
{
    std::stringstream help_cmd_launch;
    send_command({"help", "launch"}, help_cmd_launch);

    std::stringstream launch_cmd_help;
    send_command({"launch", "-h"}, launch_cmd_help);

    EXPECT_THAT(help_cmd_launch.str(), Ne(""));
    EXPECT_THAT(help_cmd_launch.str(), Eq(launch_cmd_help.str()));
}

// clone cli tests

TEST_F(Client, cloneCmdWithTooManyArgsFails)
{
    EXPECT_EQ(send_command({"clone", "vm1", "vm2"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, cloneCmdWithTooLessArgsFails)
{
    EXPECT_EQ(send_command({"clone"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, cloneCmdInvalidOptionFails)
{
    EXPECT_EQ(send_command({"clone", "--invalid_option"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, cloneCmdHelpOk)
{
    EXPECT_EQ(send_command({"clone", "--help"}), mp::ReturnCode::Ok);
}

TEST_F(Client, cloneCmdWithSrcVMNameOnly)
{
    EXPECT_CALL(mock_daemon, clone).Times(1);
    EXPECT_EQ(send_command({"clone", "vm1"}), mp::ReturnCode::Ok);
}

TEST_F(Client, cloneCmdWithDestName)
{
    EXPECT_CALL(mock_daemon, clone).Times(2);
    EXPECT_EQ(send_command({"clone", "vm1", "-n", "vm2"}), mp::ReturnCode::Ok);
    EXPECT_EQ(send_command({"clone", "vm1", "--name", "vm2"}), mp::ReturnCode::Ok);
}

TEST_F(Client, cloneCmdFailedFromDaemon)
{
    const grpc::Status clone_failure{grpc::StatusCode::FAILED_PRECONDITION, "dummy_msg"};
    EXPECT_CALL(mock_daemon, clone).Times(1).WillOnce(Return(clone_failure));
    EXPECT_EQ(send_command({"clone", "vm1"}), mp::ReturnCode::CommandFail);
}

// snapshot cli tests
TEST_F(Client, snapshotCmdHelpOk)
{
    EXPECT_EQ(send_command({"snapshot", "--help"}), mp::ReturnCode::Ok);
}

TEST_F(Client, snapshotCmdNoOptionsOk)
{
    EXPECT_CALL(mock_daemon, snapshot);
    EXPECT_EQ(send_command({"snapshot", "foo"}), mp::ReturnCode::Ok);
}

TEST_F(Client, snapshotCmdNameAlternativesOk)
{
    EXPECT_CALL(mock_daemon, snapshot).Times(2);
    EXPECT_EQ(send_command({"snapshot", "-n", "bar", "foo"}), mp::ReturnCode::Ok);
    EXPECT_EQ(send_command({"snapshot", "--name", "bar", "foo"}), mp::ReturnCode::Ok);
}

TEST_F(Client, snapshotCmdNameConsumesArg)
{
    EXPECT_CALL(mock_daemon, snapshot).Times(0);
    EXPECT_EQ(send_command({"snapshot", "--name", "foo"}), mp::ReturnCode::CommandLineError);
    EXPECT_EQ(send_command({"snapshot", "-n", "foo"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, snapshotCmdCommentOptionAlternativesOk)
{
    EXPECT_CALL(mock_daemon, snapshot).Times(3);
    EXPECT_EQ(send_command({"snapshot", "--comment", "a comment", "foo"}), mp::ReturnCode::Ok);
    EXPECT_EQ(send_command({"snapshot", "-c", "a comment", "foo"}), mp::ReturnCode::Ok);
    EXPECT_EQ(send_command({"snapshot", "-m", "a comment", "foo"}), mp::ReturnCode::Ok);
}

TEST_F(Client, snapshotCmdCommentConsumesArg)
{
    EXPECT_CALL(mock_daemon, snapshot).Times(0);
    EXPECT_EQ(send_command({"snapshot", "--comment", "foo"}), mp::ReturnCode::CommandLineError);
    EXPECT_EQ(send_command({"snapshot", "-c", "foo"}), mp::ReturnCode::CommandLineError);
    EXPECT_EQ(send_command({"snapshot", "-m", "foo"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, snapshotCmdTooFewArgsFails)
{
    EXPECT_EQ(send_command({"snapshot", "-m", "Who controls the past controls the future"}),
              mp::ReturnCode::CommandLineError);
}

TEST_F(Client, snapshotCmdTooManyArgsFails)
{
    EXPECT_EQ(send_command({"snapshot", "foo", "bar"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, snapshotCmdInvalidOptionFails)
{
    EXPECT_EQ(send_command({"snapshot", "--snap"}), mp::ReturnCode::CommandLineError);
}

// restore cli tests
TEST_F(Client, restoreCmdHelpOk)
{
    EXPECT_EQ(send_command({"restore", "--help"}), mp::ReturnCode::Ok);
}

TEST_F(Client, restoreCmdGoodArgsOk)
{
    EXPECT_CALL(mock_daemon, restore);
    EXPECT_EQ(send_command({"restore", "foo.snapshot1", "--destructive"}), mp::ReturnCode::Ok);
}

TEST_F(Client, restoreCmdTooFewArgsFails)
{
    EXPECT_EQ(send_command({"restore", "--destructive"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, restoreCmdTooManyArgsFails)
{
    EXPECT_EQ(send_command({"restore", "foo.snapshot1", "bar.snapshot2"}),
              mp::ReturnCode::CommandLineError);
}

TEST_F(Client, restoreCmdMissingInstanceFails)
{
    EXPECT_EQ(send_command({"restore", ".snapshot1"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, restoreCmdMissingSnapshotFails)
{
    EXPECT_EQ(send_command({"restore", "foo."}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, restoreCmdInvalidOptionFails)
{
    EXPECT_EQ(send_command({"restore", "--foo"}), mp::ReturnCode::CommandLineError);
}

struct RestoreCommandClient : public Client
{
    RestoreCommandClient()
    {
        EXPECT_CALL(mock_terminal, cout).WillRepeatedly(ReturnRef(cout));
        EXPECT_CALL(mock_terminal, cerr).WillRepeatedly(ReturnRef(cerr));
        EXPECT_CALL(mock_terminal, cin).WillRepeatedly(ReturnRef(cin));
    }

    std::ostringstream cerr, cout;
    std::istringstream cin;
    mpt::MockTerminal mock_terminal;
};

TEST_F(RestoreCommandClient, restoreCmdConfirmsDesruction)
{
    cin.str("invalid input\nyes\n");

    EXPECT_CALL(mock_terminal, cin_is_live()).WillOnce(Return(true));
    EXPECT_CALL(mock_terminal, cout_is_live()).WillOnce(Return(true));

    EXPECT_CALL(mock_daemon, restore(_, _)).WillOnce([](auto, auto* server) {
        mp::RestoreRequest request;
        server->Read(&request);

        EXPECT_FALSE(request.destructive());

        mp::RestoreReply reply;
        reply.set_confirm_destructive(true);
        server->Write(reply);
        return grpc::Status{};
    });

    EXPECT_EQ(setup_client_and_run({"restore", "foo.snapshot1"}, mock_terminal),
              mp::ReturnCode::Ok);
    EXPECT_TRUE(cout.str().find("Please answer yes/no"));
}

TEST_F(RestoreCommandClient, restoreCmdNotDestructiveNotLiveTermFails)
{
    EXPECT_CALL(mock_terminal, cin_is_live()).WillOnce(Return(false));

    EXPECT_CALL(mock_daemon, restore(_, _)).WillOnce([](auto, auto* server) {
        mp::RestoreReply reply;
        reply.set_confirm_destructive(true);
        server->Write(reply);
        return grpc::Status{};
    });

    EXPECT_THROW(setup_client_and_run({"restore", "foo.snapshot1"}, mock_terminal),
                 std::runtime_error);
}

// authenticate cli tests
TEST_F(Client, authenticateCmdGoodPassphraseOk)
{
    EXPECT_CALL(mock_daemon, authenticate);
    EXPECT_EQ(send_command({"authenticate", "foo"}), mp::ReturnCode::Ok);
}

TEST_F(Client, authenticateCmdInvalidOptionFails)
{
    EXPECT_EQ(send_command({"authenticate", "--foo"}), mp::ReturnCode::CommandLineError);
}

TEST_F(Client, authenticateCmdHelpOk)
{
    EXPECT_EQ(send_command({"authenticate", "--help"}), mp::ReturnCode::Ok);
}

TEST_F(Client, authenticateCmdTooManyArgsFails)
{
    EXPECT_EQ(send_command({"authenticate", "foo", "bar"}), mp::ReturnCode::CommandLineError);
}

struct AuthenticateCommandClient : public Client
{
    AuthenticateCommandClient()
    {
        EXPECT_CALL(mock_terminal, cout).WillRepeatedly(ReturnRef(cout));
        EXPECT_CALL(mock_terminal, cerr).WillRepeatedly(ReturnRef(cerr));
        EXPECT_CALL(mock_terminal, cin).WillRepeatedly(ReturnRef(cin));

        {
            InSequence s;

            EXPECT_CALL(mock_terminal, set_cin_echo(false)).Times(1);
            EXPECT_CALL(mock_terminal, set_cin_echo(true)).Times(1);
        }
    }

    std::ostringstream cerr, cout;
    std::istringstream cin;
    mpt::MockTerminal mock_terminal;
};

TEST_F(AuthenticateCommandClient, authenticateCmdAcceptsEnteredPassphrase)
{
    const std::string passphrase{"foo"};

    cin.str(passphrase + "\n");

    EXPECT_CALL(mock_daemon, authenticate(_, _)).WillOnce([&passphrase](auto, auto* server) {
        mp::AuthenticateRequest request;
        server->Read(&request);

        EXPECT_EQ(request.passphrase(), passphrase);
        return grpc::Status{};
    });

    EXPECT_EQ(setup_client_and_run({"authenticate"}, mock_terminal), mp::ReturnCode::Ok);
}

TEST_F(AuthenticateCommandClient, authenticateCmdNoPassphraseEnteredReturnsError)
{
    cin.str("\n");

    EXPECT_EQ(setup_client_and_run({"authenticate"}, mock_terminal),
              mp::ReturnCode::CommandLineError);

    EXPECT_EQ(cerr.str(), "No passphrase given\n");
}

TEST_F(AuthenticateCommandClient, authenticateCmdNoPassphrasePrompterFailsReturnsError)
{
    cin.clear();

    EXPECT_EQ(setup_client_and_run({"authenticate"}, mock_terminal),
              mp::ReturnCode::CommandLineError);

    EXPECT_EQ(cerr.str(), "Failed to read value\n");
}

const std::vector<std::string> timeout_commands{"launch",
                                                "start",
                                                "restart",
                                                "shell",
                                                "wait-ready"};
const std::vector<std::string> valid_timeouts{"120", "1234567"};
const std::vector<std::string> invalid_timeouts{"-1", "0", "a", "3min", "15.51", ""};

struct TimeoutCorrectSuite : Client, WithParamInterface<std::tuple<std::string, std::string>>
{
};

TEST_P(TimeoutCorrectSuite, cmdsWithTimeoutOk)
{
    const auto& [command, timeout] = GetParam();

    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, wait_ready).Times(AtMost(1));
    EXPECT_THAT(send_command({command, "--timeout", timeout}), Eq(mp::ReturnCode::Ok));
}

INSTANTIATE_TEST_SUITE_P(Client,
                         TimeoutCorrectSuite,
                         Combine(ValuesIn(timeout_commands), ValuesIn(valid_timeouts)));

struct TimeoutNullSuite : Client, WithParamInterface<std::string>
{
};

TEST_P(TimeoutNullSuite, cmdsWithTimeoutNullBad)
{
    EXPECT_THAT(send_command({GetParam(), "--timeout"}), Eq(mp::ReturnCode::CommandLineError));
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutNullSuite, ValuesIn(timeout_commands));

struct TimeoutInvalidSuite : Client, WithParamInterface<std::tuple<std::string, std::string>>
{
};

TEST_P(TimeoutInvalidSuite, cmdsWithInvalidTimeoutBad)
{
    std::stringstream cerr_stream;
    const auto& [command, timeout] = GetParam();

    EXPECT_THAT(send_command({command, "--timeout", timeout}, trash_stream, cerr_stream),
                Eq(mp::ReturnCode::CommandLineError));

    EXPECT_EQ(cerr_stream.str(), "error: --timeout value has to be a positive integer\n");
}

INSTANTIATE_TEST_SUITE_P(Client,
                         TimeoutInvalidSuite,
                         Combine(ValuesIn(timeout_commands), ValuesIn(invalid_timeouts)));

struct TimeoutSuite : Client, WithParamInterface<std::string>
{
    void SetUp() override
    {
        Client::SetUp();

        ON_CALL(mock_daemon, launch)
            .WillByDefault(request_sleeper<mp::LaunchRequest, mp::LaunchReply>);
        ON_CALL(mock_daemon, start)
            .WillByDefault(request_sleeper<mp::StartRequest, mp::StartReply>);
        ON_CALL(mock_daemon, restart)
            .WillByDefault(request_sleeper<mp::RestartRequest, mp::RestartReply>);
        ON_CALL(mock_daemon, ssh_info)
            .WillByDefault(request_sleeper<mp::SSHInfoRequest, mp::SSHInfoReply>);
        ON_CALL(mock_daemon, wait_ready)
            .WillByDefault(request_sleeper<mp::WaitReadyRequest, mp::WaitReadyReply>);
    }

    template <typename RequestType, typename ReplyType>
    static grpc::Status request_sleeper(grpc::ServerContext* context,
                                        grpc::ServerReaderWriter<ReplyType, RequestType>* response)
    {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return grpc::Status::OK;
    }
};

TEST_P(TimeoutSuite, commandExitsOnTimeout)
{

    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, wait_ready).Times(AtMost(1));
    EXPECT_CALL(*mock_utils, exit(mp::timeout_exit_code));

    send_command({GetParam(), "--timeout", "1"});
}

TEST_P(TimeoutSuite, commandCompletesWithoutTimeout)
{
    EXPECT_CALL(mock_daemon, launch).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, start).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, ssh_info).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, wait_ready).Times(AtMost(1));

    EXPECT_EQ(send_command({GetParam(), "--timeout", "5"}), mp::ReturnCode::Ok);
}

INSTANTIATE_TEST_SUITE_P(Client, TimeoutSuite, ValuesIn(timeout_commands));

struct ClientLogMessageSuite : Client, WithParamInterface<std::vector<std::string>>
{
    void SetUp() override
    {
        Client::SetUp();

        ON_CALL(mock_daemon, launch)
            .WillByDefault(reply_log_message<mp::LaunchReply, mp::LaunchRequest>);
        ON_CALL(mock_daemon, mount)
            .WillByDefault(reply_log_message<mp::MountReply, mp::MountRequest>);
        ON_CALL(mock_daemon, start)
            .WillByDefault(reply_log_message<mp::StartReply, mp::StartRequest>);
        ON_CALL(mock_daemon, restart)
            .WillByDefault(reply_log_message<mp::RestartReply, mp::RestartRequest>);
        ON_CALL(mock_daemon, version)
            .WillByDefault(reply_log_message<mp::VersionReply, mp::VersionRequest>);
        ON_CALL(mock_daemon, wait_ready)
            .WillByDefault(reply_log_message<mp::WaitReadyReply, mp::WaitReadyRequest>);
    }

    template <typename ReplyType, typename RequestType>
    static grpc::Status
    reply_log_message(Unused, grpc::ServerReaderWriter<ReplyType, RequestType>* response)
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
    EXPECT_CALL(mock_daemon, restart).Times(AtMost(1));
    EXPECT_CALL(mock_daemon, wait_ready).Times(AtMost(1));

    std::stringstream cerr_stream;

    send_command(GetParam(), trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), log_message);
}

INSTANTIATE_TEST_SUITE_P(Client,
                         ClientLogMessageSuite,
                         Values(std::vector<std::string>{"launch"},
                                std::vector<std::string>{"mount", "..", "test-vm:test"},
                                std::vector<std::string>{"start"},
                                std::vector<std::string>{"version"},
                                std::vector<std::string>{"restart"},
                                std::vector<std::string>{"version"},
                                std::vector<std::string>{"wait-ready"}));

TEST_F(ClientAlias, aliasCreatesAlias)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}}});

    EXPECT_EQ(
        send_command(
            {"alias", "primary:another_command", "another_alias", "--no-map-working-directory"}),
        mp::ReturnCode::Ok);

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(),
                csv_header + "an_alias,an_instance,a_command,map,default*\n"
                             "another_alias,primary,another_command,default,default*\n");
}

struct ClientAliasNameSuite
    : public ClientAlias,
      public WithParamInterface<std::tuple<std::string /* command */, std::string /* path */>>
{
};

TEST_P(ClientAliasNameSuite, createsCorrectDefaultAliasName)
{
    const auto& [command, path] = GetParam();

    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::vector<std::string> arguments{"alias"};
    arguments.push_back("primary:" + path + command);
    arguments.push_back("--no-map-working-directory");

    EXPECT_EQ(send_command(arguments), mp::ReturnCode::Ok);

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(),
                fmt::format(fmt::runtime(csv_header + "{},primary,{}{},default,default*\n"),
                            command,
                            path,
                            command));
}

INSTANTIATE_TEST_SUITE_P(
    ClientAlias,
    ClientAliasNameSuite,
    Combine(Values("command", "com.mand", "com.ma.nd"),
            Values("", "/", "./", "./relative/", "/absolute/", "../more/relative/")));

TEST_F(ClientAlias, failsIfCannotWriteFullyQualifiedScript)
{
    EXPECT_CALL(*mock_platform, create_alias_script(_, _))
        .Times(1)
        .WillRepeatedly(Throw(std::runtime_error("aaa")));

    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"alias", "primary:command"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Error when creating script for alias: aaa\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header);
}

TEST_F(ClientAlias, failsIfCannotWriteNonFullyQualifiedScript)
{
    EXPECT_CALL(*mock_platform, create_alias_script(_, _))
        .WillOnce(Return())
        .WillOnce(Throw(std::runtime_error("bbb")));
    EXPECT_CALL(*mock_platform, remove_alias_script(_)).WillOnce(Return());

    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"alias", "primary:command"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Error when creating script for alias: bbb\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header);
}

TEST_F(ClientAlias, aliasDoesNotOverwriteAlias)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(
        send_command({"alias", "primary:another_command", "an_alias"}, trash_stream, cerr_stream),
        mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Alias 'an_alias' already exists in current context\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header + "an_alias,an_instance,a_command,map,default*\n");
}

struct ArgumentCheckTestsuite
    : public ClientAlias,
      public WithParamInterface<
          std::tuple<std::vector<std::string>, mp::ReturnCode, std::string, std::string>>
{
};

TEST_P(ArgumentCheckTestsuite, answersCorrectly)
{
    auto [arguments, expected_return_code, expected_cout, expected_cerr] = GetParam();

    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::stringstream cout_stream, cerr_stream;
    EXPECT_EQ(send_command(arguments, cout_stream, cerr_stream), expected_return_code);

    EXPECT_THAT(cout_stream.str(), HasSubstr(expected_cout));
    EXPECT_EQ(cerr_stream.str(), expected_cerr);
}

INSTANTIATE_TEST_SUITE_P(
    Client,
    ArgumentCheckTestsuite,
    Values(
        std::make_tuple(std::vector<std::string>{"alias"},
                        mp::ReturnCode::CommandLineError,
                        "",
                        "Wrong number of arguments given\n"),
        std::make_tuple(std::vector<std::string>{"alias", "instance", "command", "alias_name"},
                        mp::ReturnCode::CommandLineError,
                        "",
                        "Wrong number of arguments given\n"),
        std::make_tuple(std::vector<std::string>{"alias", "instance", "alias_name"},
                        mp::ReturnCode::CommandLineError,
                        "",
                        "No command given\n"),
        std::make_tuple(std::vector<std::string>{"alias", "primary:command", "alias_name"},
                        mp::ReturnCode::Ok,
                        "You'll need to add",
                        ""),
        std::make_tuple(std::vector<std::string>{"alias", "primary:command"},
                        mp::ReturnCode::Ok,
                        "You'll need to add",
                        ""),
        std::make_tuple(std::vector<std::string>{"alias", ":command"},
                        mp::ReturnCode::CommandLineError,
                        "",
                        "No instance name given\n"),
        std::make_tuple(std::vector<std::string>{"alias", ":command", "alias_name"},
                        mp::ReturnCode::CommandLineError,
                        "",
                        "No instance name given\n"),
        std::make_tuple(std::vector<std::string>{"alias", "primary:command", "relative/alias_name"},
                        mp::ReturnCode::CommandLineError,
                        "",
                        "Alias has to be a valid filename\n"),
        std::make_tuple(
            std::vector<std::string>{"alias", "primary:command", "/absolute/alias_name"},
            mp::ReturnCode::CommandLineError,
            "",
            "Alias has to be a valid filename\n"),
        std::make_tuple(std::vector<std::string>{"alias", "primary:command", "weird alias_name"},
                        mp::ReturnCode::Ok,
                        "You'll need to add",
                        ""),
        std::make_tuple(std::vector<std::string>{"alias", "primary:command", "com.mand"},
                        mp::ReturnCode::Ok,
                        "You'll need to add",
                        ""),
        std::make_tuple(std::vector<std::string>{"alias", "primary:command", "com.ma.nd"},
                        mp::ReturnCode::Ok,
                        "You'll need to add",
                        "")));

TEST_F(ClientAlias, emptyAliases)
{
    std::stringstream cout_stream;
    send_command({"aliases"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), "No aliases defined.\n");
}

TEST_F(ClientAlias, badAliasesFormat)
{
    std::stringstream cerr_stream;
    send_command({"aliases", "--format", "wrong"}, trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), "Invalid format type given.\n");
}

TEST_F(ClientAlias, tooManyAliasesArguments)
{
    std::stringstream cerr_stream;
    send_command({"aliases", "bad_argument"}, trash_stream, cerr_stream);

    EXPECT_EQ(cerr_stream.str(), "This command takes no arguments\n");
}

TEST_F(ClientAlias, executeExistingAlias)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command", "map"}}});

    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());
    EXPECT_CALL(mock_daemon, ssh_info(_, _));

    EXPECT_EQ(send_command({"some_alias"}), mp::ReturnCode::Ok);
}

TEST_F(ClientAlias, executeNonexistentAlias)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command", "map"}}});

    EXPECT_CALL(mock_daemon, ssh_info(_, _)).Times(0);

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"other_undefined_alias"}, cout_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_THAT(cout_stream.str(), HasSubstr("Unknown command or alias"));
}

TEST_F(ClientAlias, executeAliasWithArguments)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command", "map"}}});

    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());
    EXPECT_CALL(mock_daemon, ssh_info(_, _));

    EXPECT_EQ(send_command({"some_alias", "some_argument"}), mp::ReturnCode::Ok);
}

TEST_F(ClientAlias, failsExecutingAliasWithoutSeparator)
{
    populate_db_file(AliasesVector{{"some_alias", {"some_instance", "some_command", "map"}}});

    EXPECT_CALL(mock_daemon, ssh_info(_, _)).Times(0);

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"some_alias", "--some-option"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_THAT(cerr_stream.str(),
                HasSubstr("Options to the alias should come after \"--\", like this:\n"
                          "multipass <alias> -- <arguments>\n"));
}

TEST_F(ClientAlias, aliasRefusesCreationNonexistentInstance)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}}});

    std::stringstream cout_stream, cerr_stream;
    send_command({"alias", "foo:another_command", "another_alias"}, cout_stream, cerr_stream);

    EXPECT_EQ(cout_stream.str(), "");
    EXPECT_EQ(cerr_stream.str(), "Instance 'foo' does not exist\n");

    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header + "an_alias,an_instance,a_command,map,default*\n");
}

TEST_F(ClientAlias, aliasRefusesCreationRpcError)
{
    EXPECT_CALL(mock_daemon, info(_, _))
        .WillOnce(Return(grpc::Status{grpc::StatusCode::NOT_FOUND, "msg"}));

    populate_db_file(AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}}});

    std::stringstream cout_stream, cerr_stream;
    send_command({"alias", "foo:another_command", "another_alias"}, cout_stream, cerr_stream);

    EXPECT_EQ(cout_stream.str(), "");
    EXPECT_EQ(cerr_stream.str(), "Error retrieving list of instances\n");

    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header + "an_alias,an_instance,a_command,map,default*\n");
}

// zones cli tests
TEST_F(Client, zones_cmd_no_args_ok)
{
    EXPECT_CALL(mock_daemon, zones(_, _));
    EXPECT_THAT(send_command({"zones"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, zones_cmd_help_ok)
{
    EXPECT_THAT(send_command({"zones", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, zones_cmd_succeeds_with_format_options)
{
    EXPECT_CALL(mock_daemon, zones(_, _)).Times(4);
    EXPECT_THAT(send_command({"zones", "--format", "table"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"zones", "--format", "json"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"zones", "--format", "csv"}), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(send_command({"zones", "--format", "yaml"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, zones_cmd_fails_with_invalid_format)
{
    EXPECT_THAT(send_command({"zones", "--format", "invalid"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, zones_cmd_fails_with_args)
{
    std::stringstream cerr_stream;
    EXPECT_THAT(send_command({"zones", "foo"}, trash_stream, cerr_stream), Eq(mp::ReturnCode::CommandLineError));
    EXPECT_THAT(cerr_stream.str(), HasSubstr("This command takes no arguments"));
}

TEST_F(Client, zones_cmd_verbosity_forwarded)
{
    const auto verbosity = 2;
    const auto request_matcher = make_request_verbosity_matcher<mp::ZonesRequest>(verbosity);

    EXPECT_CALL(mock_daemon, zones)
        .WillOnce(WithArg<1>(check_request_and_return<mp::ZonesReply, mp::ZonesRequest>(request_matcher, ok)));
    EXPECT_THAT(send_command({"zones", "-vv"}), Eq(mp::ReturnCode::Ok));
}

// enable_zones tests
TEST_F(Client, enable_zones_cmd_help_ok)
{
    EXPECT_THAT(send_command({"enable-zones", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, enable_zones_cmd_success)
{
    EXPECT_CALL(mock_daemon, zones_state(_, _));
    EXPECT_THAT(send_command({"enable-zones", "zone1", "zone2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, enable_zones_cmd_no_zones_fails)
{
    EXPECT_CALL(mock_daemon, zones_state(_, _)).Times(0);
    EXPECT_THAT(send_command({"enable-zones"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, enable_zones_cmd_passes_proper_request)
{
    const auto request_matcher = AllOf(Property(&mp::ZonesStateRequest::available, true),
                                       Property(&mp::ZonesStateRequest::zones, ElementsAre("zone1", "zone2")));

    EXPECT_CALL(mock_daemon, zones_state)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::ZonesStateReply, mp::ZonesStateRequest>(request_matcher, ok)));

    EXPECT_THAT(send_command({"enable-zones", "zone1", "zone2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, enable_zones_cmd_on_failure)
{
    const auto failure = grpc::Status{grpc::StatusCode::UNAVAILABLE, "msg"};
    EXPECT_CALL(mock_daemon, zones_state(_, _)).WillOnce(Return(failure));
    EXPECT_THAT(send_command({"enable-zones", "zone1"}), Eq(mp::ReturnCode::CommandFail));
}

// disable_zones tests
TEST_F(Client, disable_zones_cmd_help_ok)
{
    EXPECT_THAT(send_command({"disable-zones", "-h"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, disable_zones_cmd_success)
{
    EXPECT_CALL(mock_daemon, zones_state(_, _));
    EXPECT_THAT(send_command({"disable-zones", "--force", "zone1", "zone2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, disable_zones_cmd_no_zones_fails)
{
    EXPECT_CALL(mock_daemon, zones_state(_, _)).Times(0);
    EXPECT_THAT(send_command({"disable-zones"}), Eq(mp::ReturnCode::CommandLineError));
}

TEST_F(Client, disable_zones_cmd_passes_proper_request)
{
    const auto request_matcher = AllOf(Property(&mp::ZonesStateRequest::available, false),
                                       Property(&mp::ZonesStateRequest::zones, ElementsAre("zone1", "zone2")));

    EXPECT_CALL(mock_daemon, zones_state)
        .WillOnce(
            WithArg<1>(check_request_and_return<mp::ZonesStateReply, mp::ZonesStateRequest>(request_matcher, ok)));

    EXPECT_THAT(send_command({"disable-zones", "--force", "zone1", "zone2"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, disable_zones_cmd_with_force_option)
{
    EXPECT_CALL(mock_daemon, zones_state(_, _));
    EXPECT_THAT(send_command({"disable-zones", "--force", "zone1"}), Eq(mp::ReturnCode::Ok));
}

TEST_F(Client, disable_zones_cmd_confirm)
{
    std::stringstream cout, cerr;
    std::istringstream cin;
    mpt::MockTerminal term;
    EXPECT_CALL(term, cout()).WillRepeatedly(ReturnRef(cout));
    EXPECT_CALL(term, cerr()).WillRepeatedly(ReturnRef(cerr));
    EXPECT_CALL(term, cin()).WillRepeatedly(ReturnRef(cin));
    EXPECT_CALL(term, cin_is_live()).WillRepeatedly(Return(true));
    EXPECT_CALL(term, cout_is_live()).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_daemon, zones_state(_, _));

    cin.str("yes\n");
    EXPECT_THAT(setup_client_and_run({"disable-zones", "zone1"}, term), Eq(mp::ReturnCode::Ok));

    cin.str("no\n");
    EXPECT_THAT(setup_client_and_run({"disable-zones", "zone1"}, term), Eq(mp::ReturnCode::CommandFail));
}

TEST_F(Client, disable_zones_cmd_on_failure)
{
    const auto failure = grpc::Status{grpc::StatusCode::UNAVAILABLE, "msg"};
    EXPECT_CALL(mock_daemon, zones_state(_, _)).WillOnce(Return(failure));
    EXPECT_THAT(send_command({"disable-zones", "--force", "zone1"}), Eq(mp::ReturnCode::CommandFail));
}
TEST_F(Client, disable_zones_cmd_not_live_term_fails)
{
    NiceMock<mpt::MockTerminal> term;
    EXPECT_CALL(term, cin_is_live()).WillRepeatedly(Return(false));
    EXPECT_CALL(term, cout_is_live()).WillRepeatedly(Return(true));

    EXPECT_THROW(setup_client_and_run({"disable-zones", "zone1"}, term), std::runtime_error);
}

TEST_F(Client, disable_zones_cmd_confirm_multiple_zones)
{
    NiceMock<mpt::MockTerminal> term;
    std::stringstream cin_stream;
    cin_stream << "Yes\n";
    ON_CALL(term, cin()).WillByDefault(ReturnRef(cin_stream));
    ON_CALL(term, cin_is_live()).WillByDefault(Return(true));
    ON_CALL(term, cout_is_live()).WillByDefault(Return(true));

    std::stringstream cout_stream;
    ON_CALL(term, cout()).WillByDefault(ReturnRef(cout_stream));
    std::stringstream cerr_stream;
    ON_CALL(term, cerr()).WillByDefault(ReturnRef(cerr_stream));

    EXPECT_CALL(mock_daemon,
                zones_state(An<grpc::ServerContext*>(),
                            An<grpc::ServerReaderWriter<mp::ZonesStateReply, mp::ZonesStateRequest>*>()))
        .WillOnce(Return(grpc::Status::OK));
    EXPECT_THAT(setup_client_and_run({"disable-zones", "zone1", "zone2", "zone3"}, term), Eq(mp::ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(),
                HasSubstr("This operation will forcefully stop the VMs in zone1, zone2 and zone3. Are you sure you "
                          "want to continue? (Yes/no)"));
}

TEST_F(ClientAlias, aliasRefusesCreateDuplicateAlias)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    populate_db_file(AliasesVector{{"an_alias", {"primary", "a_command", "map"}}});

    std::stringstream cout_stream, cerr_stream;
    send_command({"alias", "primary:another_command", "an_alias"}, cout_stream, cerr_stream);

    EXPECT_EQ(cout_stream.str(), "");
    EXPECT_EQ(cerr_stream.str(), "Alias 'an_alias' already exists in current context\n");

    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header + "an_alias,primary,a_command,map,default*\n");
}

TEST_F(ClientAlias, aliasCreatesAliasThatExistsInAnotherContext)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    populate_db_file(AliasesVector{{"an_alias", {"primary", "a_command", "map"}}});

    EXPECT_EQ(send_command({"prefer", "new_context"}), mp::ReturnCode::Ok);

    std::stringstream cout_stream, cerr_stream;
    EXPECT_EQ(
        send_command({"alias", "primary:another_command", "an_alias"}, cout_stream, cerr_stream),
        mp::ReturnCode::Ok);

    EXPECT_EQ(cout_stream.str(), "");
    EXPECT_EQ(cerr_stream.str(), "");

    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(),
                csv_header + "an_alias,primary,a_command,map,default\nan_alias,primary,another_"
                             "command,map,new_context*\n");
}

TEST_F(ClientAlias, unaliasRemovesExistingAlias)
{
    populate_db_file(
        AliasesVector{{"an_alias", {"an_instance", "a_command", "default"}},
                      {"another_alias", {"another_instance", "another_command", "map"}}});

    EXPECT_EQ(send_command({"unalias", "another_alias"}), mp::ReturnCode::Ok);

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(),
                csv_header + "an_alias,an_instance,a_command,default,default*\n");
}

TEST_F(ClientAlias, unaliasSucceedsEvenIfScriptCannotBeRemoved)
{
    EXPECT_CALL(*mock_platform, remove_alias_script(_))
        .Times(1)
        .WillRepeatedly(Throw(std::runtime_error("bbb")));

    populate_db_file(
        AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}},
                      {"another_alias", {"another_instance", "another_command", "default"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "another_alias"}, trash_stream, cerr_stream),
              mp::ReturnCode::Ok);
    EXPECT_THAT(cerr_stream.str(),
                Eq("Warning: 'bbb' when removing alias script for default.another_alias\n"));

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_THAT(cout_stream.str(), csv_header + "an_alias,an_instance,a_command,map,default*\n");
}

TEST_F(ClientAlias, unaliasDoesNotRemoveNonexistentAlias)
{
    populate_db_file(
        AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}},
                      {"another_alias", {"another_instance", "another_command", "default"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "nonexistent_alias"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Nonexistent alias: nonexistent_alias.\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_EQ(cout_stream.str(),
              csv_header + "an_alias,an_instance,a_command,map,default*\n"
                           "another_alias,another_instance,another_command,default,default*\n");
}

TEST_F(ClientAlias, unaliasDoesNotRemoveNonexistentAliases)
{
    populate_db_file(
        AliasesVector{{"an_alias", {"an_instance", "a_command", "default"}},
                      {"another_alias", {"another_instance", "another_command", "map"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "nonexistent_alias", "another_nonexistent_alias"},
                           trash_stream,
                           cerr_stream),
              mp::ReturnCode::CommandLineError);
    // Since the container for bad aliases is unordered, we cannot expect an ordered output.
    EXPECT_THAT(cerr_stream.str(), HasSubstr("Nonexistent aliases: "));
    EXPECT_THAT(cerr_stream.str(), HasSubstr("nonexistent_alias"));
    EXPECT_THAT(cerr_stream.str(), HasSubstr(", "));
    EXPECT_THAT(cerr_stream.str(), HasSubstr("another_nonexistent_alias"));
    EXPECT_THAT(cerr_stream.str(), HasSubstr(".\n"));

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_EQ(cout_stream.str(),
              csv_header + "an_alias,an_instance,a_command,default,default*\n"
                           "another_alias,another_instance,another_command,map,default*\n");
}

TEST_F(ClientAlias, unaliasDashDashAllWorks)
{
    populate_db_file(
        AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}},
                      {"another_alias", {"another_instance", "another_command", "default"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "--all"}, trash_stream, cerr_stream), mp::ReturnCode::Ok);
    EXPECT_EQ(cerr_stream.str(), "");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_EQ(cout_stream.str(), csv_header);
}

TEST_F(ClientAlias, unaliasDashDashAllClashesWithOtherArguments)
{
    populate_db_file(
        AliasesVector{{"an_alias", {"an_instance", "a_command", "map"}},
                      {"another_alias", {"another_instance", "another_command", "default"}}});

    std::stringstream cerr_stream;
    EXPECT_EQ(send_command({"unalias", "arg", "--all"}, trash_stream, cerr_stream),
              mp::ReturnCode::CommandLineError);
    EXPECT_EQ(cerr_stream.str(), "Cannot specify name when --all option set\n");

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    EXPECT_EQ(cout_stream.str(),
              csv_header + "an_alias,an_instance,a_command,map,default*\n"
                           "another_alias,another_instance,another_command,default,default*\n");
}

TEST_F(ClientAlias, failsIfUnableToCreateDirectory)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, exists(A<const QFile&>())).WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(false));
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "alias_name"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), HasSubstr("Could not create path"));
}

TEST_F(ClientAlias, creatingFirstAliasDisplaysMessage)
{
    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"alias", "primary:a_command", "an_alias"}, cout_stream),
              mp::ReturnCode::Ok);

    EXPECT_THAT(cout_stream.str(), HasSubstr("You'll need to add "));
}

TEST_F(ClientAlias, creatingFirstAliasDoesNotDisplayMessageIfPathIsSet)
{
    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function());

    auto path = qgetenv("PATH");
#ifdef MULTIPASS_PLATFORM_WINDOWS
    path += ';';
#else
    path += ':';
#endif
    path += MP_PLATFORM.get_alias_scripts_folder().path().toUtf8();
    const auto env_scope = mpt::SetEnvScope{"PATH", path};

    std::stringstream cout_stream;
    EXPECT_EQ(send_command({"alias", "primary:a_command", "an_alias"}, cout_stream),
              mp::ReturnCode::Ok);

    EXPECT_THAT(cout_stream.str(), Eq(""));
}

TEST_F(ClientAlias, failsWhenNameClashesWithCommandAlias)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "ls"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), Eq("Alias name 'ls' clashes with a command name\n"));
}

TEST_F(ClientAlias, failsWhenNameClashesWithCommandName)
{
    EXPECT_CALL(mock_daemon, info(_, _)).Times(AtMost(1)).WillRepeatedly(make_info_function());

    std::stringstream cerr_stream;
    send_command({"alias", "primary:command", "list"}, trash_stream, cerr_stream);

    ASSERT_THAT(cerr_stream.str(), Eq("Alias name 'list' clashes with a command name\n"));
}

TEST_F(ClientAlias, execAliasRewritesMountedDir)
{
    std::string alias_name{"an_alias"};
    std::string instance_name{"primary"};
    std::string cmd{"pwd"};

    QDir current_dir{QDir::current()};
    std::string source_dir{(current_dir.canonicalPath()).toStdString()};
    std::string target_dir{"/home/ubuntu/dir"};

    EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function(source_dir, target_dir));

    populate_db_file(AliasesVector{{alias_name, {instance_name, cmd, "map"}}});

    REPLACE(ssh_channel_request_exec, ([&target_dir, &cmd](ssh_channel, const char* raw_cmd) {
                EXPECT_THAT(raw_cmd, StartsWith("cd " + target_dir + "/"));
                EXPECT_THAT(raw_cmd, HasSubstr("&&"));
                EXPECT_THAT(raw_cmd,
                            EndsWith(cmd)); // assuming that cmd does not have escaped characters!

                return SSH_OK;
            }));

    mp::SSHInfoReply ssh_info_response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce([&ssh_info_response](
                      grpc::ServerContext* context,
                      grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            server->Write(ssh_info_response);

            return grpc::Status{};
        });

    EXPECT_EQ(send_command({alias_name}), mp::ReturnCode::Ok);
}

struct NotDirRewriteTestsuite : public ClientAlias,
                                public WithParamInterface<std::pair<bool, QString>>
{
};

TEST_P(NotDirRewriteTestsuite, execAliasDoesNotRewriteMountedDir)
{
    auto [map_dir, source_qdir] = GetParam();

    std::string alias_name{"an_alias"};
    std::string instance_name{"primary"};
    std::string cmd{"pwd"};

    QDir current_dir{QDir::current()};
    std::string source_dir{source_qdir.toStdString()};
    std::string target_dir{"/home/ubuntu/dir"};

    if (map_dir)
        EXPECT_CALL(mock_daemon, info(_, _)).WillOnce(make_info_function(source_dir, target_dir));
    else
        EXPECT_CALL(mock_daemon, info(_, _)).Times(0);

    populate_db_file(
        AliasesVector{{alias_name, {instance_name, cmd, map_dir ? "map" : "default"}}});

    REPLACE(ssh_channel_request_exec, ([&cmd](ssh_channel, const char* raw_cmd) {
                EXPECT_THAT(raw_cmd, Not(StartsWith("cd ")));
                EXPECT_THAT(raw_cmd, Not(HasSubstr("&&")));
                EXPECT_THAT(
                    raw_cmd,
                    EndsWith(cmd)); // again, assuming that cmd does not have escaped characters

                return SSH_OK;
            }));

    mp::SSHInfoReply ssh_info_response = make_fake_ssh_info_response(instance_name);

    EXPECT_CALL(mock_daemon, ssh_info(_, _))
        .WillOnce([&ssh_info_response](
                      grpc::ServerContext* context,
                      grpc::ServerReaderWriter<mp::SSHInfoReply, mp::SSHInfoRequest>* server) {
            server->Write(ssh_info_response);

            return grpc::Status{};
        });

    std::vector<std::string> arguments{alias_name};

    EXPECT_EQ(send_command(arguments), mp::ReturnCode::Ok);
}

QString current_cdup()
{
    QDir cur{QDir::current()};
    cur.cdUp();

    return cur.canonicalPath();
}

INSTANTIATE_TEST_SUITE_P(
    ClientAlias,
    NotDirRewriteTestsuite,
    Values(std::make_pair(false, QDir{QDir::current()}.canonicalPath()),
           std::make_pair(true, QDir{QDir::current()}.canonicalPath() + "/0/1/2/3/4/5/6/7/8/9"),
           std::make_pair(true, current_cdup() + "/different_name")));

TEST_F(ClientAliasNameSuite, preferWithNoArgumentFails)
{
    EXPECT_EQ(send_command({"prefer"}), mp::ReturnCode::CommandLineError);
}

TEST_F(ClientAliasNameSuite, preferWithManyArgumentsFails)
{
    EXPECT_EQ(send_command({"prefer", "arg", "arg"}), mp::ReturnCode::CommandLineError);
}

TEST_F(ClientAliasNameSuite, preferWorks)
{
    std::stringstream cout_stream;
    send_command({"aliases", "--format=yaml"}, cout_stream);
    EXPECT_THAT(cout_stream.str(), HasSubstr("active_context: default\n"));

    EXPECT_EQ(send_command({"prefer", "new_context"}), mp::ReturnCode::Ok);

    cout_stream.str(std::string());
    send_command({"aliases", "--format=yaml"}, cout_stream);
    EXPECT_THAT(cout_stream.str(), HasSubstr("active_context: new_context\n"));
}
} // namespace
