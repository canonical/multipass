#include <multipass/cli/return_codes.h>
#include <multipass/constants.h>
#include <multipass/user_messages/launch_messages.h>
#include <src/client/cli/cmd/remote_settings_handler.h>
#include <tests/unit/client_test_fixture.h>
#include <tests/unit/disabling_macros.h>
#include <tests/unit/mock_file_ops.h>
#include <tests/unit/mock_stdcin.h>
#include <tests/unit/mock_network.h>

#include <QTemporaryDir>
#include <QTimer>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace multipass::test
{

TEST_F(Client, launchCmdGoodArguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "foo"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdWrongMemArguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _)).Times(0);
    MP_EXPECT_THROW_THAT(send_command({"launch", "-m", "wrong"}),
                         std::runtime_error,
                         match_what(HasSubstr("wrong is not a valid memory size")));
    MP_EXPECT_THROW_THAT(send_command({"launch", "--memory", "1.23f"}),
                         std::runtime_error,
                         match_what(HasSubstr("1.23f is not a valid memory size")));
    MP_EXPECT_THROW_THAT(
        send_command({"launch", "-memory", "2048M"}),
        std::runtime_error,
        match_what(HasSubstr("emory is not a valid memory size"))); // note single dash
}

TEST_F(Client, launchCmdWrongDiskArguments)
{
    EXPECT_CALL(mock_daemon, launch(_, _)).Times(0);
    MP_EXPECT_THROW_THAT(send_command({"launch", "-d", "wrong"}),
                         std::runtime_error,
                         match_what(HasSubstr("wrong is not a valid memory size")));
    MP_EXPECT_THROW_THAT(send_command({"launch", "--disk", "4.56f"}),
                         std::runtime_error,
                         match_what(HasSubstr("4.56f is not a valid memory size")));
    MP_EXPECT_THROW_THAT(
        send_command({"launch", "-disk", "8192M"}),
        std::runtime_error,
        match_what(HasSubstr("isk is not a valid memory size"))); // note single dash
}

TEST_F(Client, launchCmdHelpOk)
{
    EXPECT_THAT(send_command({"launch", "-h"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdFailsMultipleArgs)
{
    EXPECT_THAT(send_command({"launch", "foo", "bar"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdFailsUnknownOption)
{
    EXPECT_THAT(send_command({"launch", "-z", "2"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdNameOptionOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "-n", "foo"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdNameOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "-n"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdMemoryOptionOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "-m", "1G"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdMemoryOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "-m"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionOk)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "-c", "2"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdCpuOptionAlphaNumericFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "w00t"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionAlphaFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "many"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionDecimalFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "1.608"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionZeroFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "0"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionNegativeFail)
{
    EXPECT_THAT(send_command({"launch", "-c", "-2"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCpuOptionFailsNoValue)
{
    EXPECT_THAT(send_command({"launch", "-c"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, DISABLE_ON_MACOS(launchCmdCustomImageFileOk))
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "file://foo"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, DISABLE_ON_MACOS(launchCmdCustomImageHttpOk))
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "http://foo"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdWithZoneOk)
{
    ZonesReply zones_reply{};
    zones_reply.add_zones()->set_name("zone1");
    EXPECT_CALL(mock_daemon, zones)
        .WillOnce(
            WithArg<1>(check_request_and_return<ZonesReply, ZonesRequest>(_, ok, zones_reply)));

    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--zone", "zone1"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdWithEmptyZoneFails)
{
    EXPECT_CALL(mock_daemon, zones).Times(0);
    EXPECT_THAT(send_command({"launch", "--zone", ""}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdWithInvalidZoneFails)
{
    ZonesReply zones_reply{};
    zones_reply.add_zones()->set_name("zone1");
    EXPECT_CALL(mock_daemon, zones)
        .Times(2)
        .WillRepeatedly(
            WithArg<1>(check_request_and_return<ZonesReply, ZonesRequest>(_, ok, zones_reply)));

    const auto request_matcher = Property(&LaunchRequest::zone, StrEq("invalid_zone"));
    EXPECT_CALL(mock_daemon, launch).Times(0);
    EXPECT_THAT(send_command({"launch", "--zone", "invalid_zone"}), Eq(ReturnCode::CommandFail));
    EXPECT_THAT(send_command({"launch",
                              "--zone",
                              "1123124029352598273598273598237529835723752039857203985720398572"}),
                Eq(ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdWithUnavailableZoneFails)
{
    ZonesReply zones_reply{};
    const auto reply_zone1 = zones_reply.add_zones();
    reply_zone1->set_name("zone1");
    reply_zone1->set_available(false);
    EXPECT_CALL(mock_daemon, zones)
        .WillOnce(
            WithArg<1>(check_request_and_return<ZonesReply, ZonesRequest>(_, ok, zones_reply)));

    const auto request_matcher = Property(&LaunchRequest::zone, StrEq("zone1"));
    LaunchError launch_error;
    launch_error.add_error_codes(LaunchError::ZONE_UNAVAILABLE);
    const auto failure = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT,
                                      "msg",
                                      launch_error.SerializeAsString()};
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<LaunchReply, LaunchRequest>(request_matcher, failure)));
    EXPECT_THAT(send_command({"launch", "--zone", "zone1"}), Eq(ReturnCode::CommandFail));
}

struct LaunchServerTimeout
    : Client,
      WithParamInterface<std::pair<ReadinessError::ErrorCode, LaunchUserMessages::MountRoutes>>
{
    void SetUp() override
    {
        Client::SetUp();

        EXPECT_CALL(mock_file_ops, exists(An<const QFileInfo&>())).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_file_ops, isDir(An<const QFileInfo&>())).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_file_ops, isReadable(An<const QFileInfo&>())).WillRepeatedly(Return(true));
    }

    void expect_launch_to_return_readiness_timeout(ReadinessError::ErrorCode readiness_error)
    {
        LaunchError launch_error;
        launch_error.set_readiness_error(readiness_error);
        grpc::Status status = {grpc::StatusCode::DEADLINE_EXCEEDED,
                               "readiness timed out",
                               launch_error.SerializeAsString()};
        EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(status));
        EXPECT_CALL(mock_daemon, mount).Times(0);
    }

    std::vector<std::string> make_launch_command(
        const LaunchUserMessages::MountRoutes& mount_routes)
    {
        std::vector<std::string> result = {"launch", "--name", instance_name};
        for (const auto& route : mount_routes)
        {
            result.push_back("--mount");
            result.push_back(fmt::format("{}:{}", route.source, route.target));
        }

        return result;
    }

    MockFileOps::GuardedMock mock_file_ops_injected = MockFileOps::inject();
    MockFileOps& mock_file_ops = *mock_file_ops_injected.first;

    const char* instance_name = "my-vm";
};

static const LaunchUserMessages::MountRoutes empty_mount_routes = {};

static const LaunchUserMessages::MountRoutes mount_routes = {
    {"/mount/source", "/mount/target"},
    {"/other/source", "/other/target"},
};

INSTANTIATE_TEST_SUITE_P(Client,
                         LaunchServerTimeout,
                         Values(std::make_pair(ReadinessError::SSH_TIMEOUT, empty_mount_routes),
                                std::make_pair(ReadinessError::CLOUD_INIT_TIMEOUT,
                                               empty_mount_routes),
                                std::make_pair(ReadinessError::SSH_TIMEOUT, mount_routes),
                                std::make_pair(ReadinessError::CLOUD_INIT_TIMEOUT, mount_routes)));

TEST_P(LaunchServerTimeout, launchServerTimeoutPrintsGuidanceAndReturnsTimeoutFailure)
{
    const auto& [readiness_error, mount_routes] = GetParam();
    expect_launch_to_return_readiness_timeout(readiness_error);

    std::stringstream cerr_stream;
    EXPECT_THAT(send_command(make_launch_command(mount_routes), trash_stream, cerr_stream),
                Eq(ReturnCode::TimeoutFailure));

    EXPECT_THAT(
        cerr_stream.str(),
        HasSubstr(
            LaunchUserMessages::server_timeout(readiness_error, instance_name, mount_routes)));
}

TEST_F(Client, launchCmdWithTimer)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--timeout", "1"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdCloudinitOptionWithValidFileIsOk)
{
    QTemporaryFile tmpfile; // file is auto-deleted when this goes out of scope
    ASSERT_TRUE(tmpfile.open());
    tmpfile.write("password: passw0rd"); // need some YAML
    tmpfile.close();
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--cloud-init", qPrintable(tmpfile.fileName())}),
                Eq(ReturnCode::Ok));
}

struct BadCloudInitFile : public Client, WithParamInterface<std::string>
{
};

TEST_P(BadCloudInitFile, launchCmdCloudinitOptionFailsWithMissingFile)
{
    std::stringstream cerr_stream;
    auto missing_file{"/definitely/missing-file"};

    EXPECT_THAT(send_command({"launch", "--cloud-init", missing_file}, trash_stream, cerr_stream),
                Eq(ReturnCode::CommandLineError));
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
    EXPECT_THAT(send_command({"launch", "--cloud-init"}), Eq(ReturnCode::CommandLineError));
}

TEST_F(Client, launchCmdCloudinitOptionReadsStdinOk)
{
    MockStdCin cin("password: passw0rd"); // no effect since terminal encapsulation of streams

    std::stringstream ss;
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_THAT(send_command({"launch", "--cloud-init", "-"}, trash_stream, trash_stream, ss),
                Eq(ReturnCode::Ok));
}

#ifndef WIN32 // TODO make home mocking work for windows
TEST_F(Client, launchCmdAutomountsHomeInPetenv)
{
    const auto fake_home = QTemporaryDir{}; // the client checks the mount source exists
    const auto env_scope = SetEnvScope{"HOME", fake_home.path().toUtf8()};
    const auto home_automount_matcher = make_mount_matcher(fake_home.path().toStdString(),
                                                           home_automount_dir,
                                                           petenv_name);
    const auto petenv_launch_matcher = make_launch_instance_matcher(petenv_name);

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<LaunchReply, LaunchRequest>(petenv_launch_matcher, ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<MountReply, MountRequest>(home_automount_matcher, ok)));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name}), Eq(ReturnCode::Ok));
}
#endif

TEST_F(Client, launchCmdSkipsAutomountWhenDisabled)
{
    std::stringstream cout_stream;
    EXPECT_CALL(mock_settings, get(Eq(mounts_key))).WillOnce(Return("false"));

    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount).Times(0);

    EXPECT_THAT(send_command({"launch", "--name", petenv_name}, cout_stream), Eq(ReturnCode::Ok));
    EXPECT_THAT(cout_stream.str(), HasSubstr("Skipping mount due to disabled mounts feature\n"));
}

TEST_F(Client, launchCmdOnlyWarnsMountForPetEnv)
{
    const auto invalid_argument = grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "msg"};
    std::stringstream cout_stream;
    EXPECT_CALL(mock_settings, get(Eq(mounts_key))).WillRepeatedly(Return("false"));
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(invalid_argument));

    EXPECT_THAT(send_command({"launch", "--name", ".asdf"}, cout_stream),
                Eq(ReturnCode::CommandFail));
    EXPECT_THAT(cout_stream.str(),
                Not(HasSubstr("Skipping mount due to disabled mounts feature\n")));
}

TEST_F(Client, launchCmdFailsWhenUnableToRetrieveAutomountSetting)
{
    const auto except = RemoteHandlerException{grpc::Status{grpc::StatusCode::INTERNAL, "oops"}};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_CALL(mock_settings, get(Eq(mounts_key))).WillOnce(Throw(except));
    EXPECT_CALL(mock_daemon, mount).Times(0);
    EXPECT_THAT(send_command({"launch", "--name", petenv_name}), Eq(ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdFailsWhenAutomountingInPetenvFails)
{
    const grpc::Status mount_failure{grpc::StatusCode::INVALID_ARGUMENT, "msg"};

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch(_, _)).WillOnce(Return(ok));
    EXPECT_CALL(mock_daemon, mount(_, _)).WillOnce(Return(mount_failure));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name}), Eq(ReturnCode::CommandFail));
}

TEST_F(Client, launchCmdForwardsVerbosityToSubcommands)
{
    const auto verbosity = 4;
    const auto launch_verbosity_matcher = make_request_verbosity_matcher<LaunchRequest>(verbosity);
    const auto mount_verbosity_matcher = make_request_verbosity_matcher<MountRequest>(verbosity);

    InSequence seq;
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(WithArg<1>(
            check_request_and_return<LaunchReply, LaunchRequest>(launch_verbosity_matcher, ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(WithArg<1>(
            check_request_and_return<MountReply, MountRequest>(mount_verbosity_matcher, ok)));
    EXPECT_THAT(send_command({"launch", "--name", petenv_name, "-vvvv"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdDoesNotAutomountInNormalInstances)
{
    EXPECT_CALL(mock_daemon, launch(_, _));
    EXPECT_CALL(mock_daemon, mount(_, _))
        .Times(0); // because we may want to move from a Strict mock in the future
    EXPECT_THAT(send_command({"launch"}), Eq(ReturnCode::Ok));
}

TEST_F(Client, launchCmdDisabledPetenvPasses)
{
    const auto custom_petenv = "";
    EXPECT_CALL(mock_settings, get(Eq(petenv_key))).WillRepeatedly(Return(custom_petenv));

    const auto petenv_matcher = make_launch_instance_matcher("foo");
    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(
            WithArg<1>(check_request_and_return<LaunchReply, LaunchRequest>(petenv_matcher, ok)));

    EXPECT_THAT(send_command({"launch", "--name", "foo"}), Eq(ReturnCode::Ok));
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
        .WillOnce(
            WithArg<1>(check_request_and_return<LaunchReply, LaunchRequest>(launch_matcher, ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(
            WithArg<1>(check_request_and_return<MountReply, MountRequest>(mount_matcher, ok)));
    EXPECT_EQ(send_command({"launch", "--name", instance_name, "--mount", fake_source}),
              ReturnCode::Ok);
}

TEST_F(Client, launchCmdMountOptionFailsOnInvalidDir)
{
    auto [mocked_file_ops, mocked_file_ops_guard] = MockFileOps::inject();
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
              ReturnCode::CommandLineError);
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
              ReturnCode::CommandLineError);
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
              ReturnCode::CommandLineError);
    EXPECT_THAT(err.str(),
                HasSubstr(fmt::format("Mount source path \"{}\" is not readable", fake_source)));
}

TEST_F(Client, launchCmdPetenvMountOptionOverrideHome)
{
    const QTemporaryDir fake_directory{};

    const auto fake_source = fake_directory.path().toStdString();
    const auto fake_target = home_automount_dir;

    const auto mount_matcher = make_mount_matcher(fake_source, fake_target, petenv_name);
    const auto launch_matcher = make_launch_instance_matcher(petenv_name);

    EXPECT_CALL(mock_daemon, launch)
        .WillOnce(
            WithArg<1>(check_request_and_return<LaunchReply, LaunchRequest>(launch_matcher, ok)));
    EXPECT_CALL(mock_daemon, mount)
        .WillOnce(
            WithArg<1>(check_request_and_return<MountReply, MountRequest>(mount_matcher, ok)));
    EXPECT_EQ(send_command({"launch",
                            "--name",
                            petenv_name,
                            "--mount",
                            fmt::format("{}:{}", fake_source, fake_target)}),
              ReturnCode::Ok);
}

TEST_F(Client, launchCmdCloudinitUrl)
{
    const auto fake_url = QStringLiteral("https://example.com");
    const auto fake_downloaded_yaml = QByteArrayLiteral("password: passw0rd");

    auto [mock_network_manager_factory, guard] = MockNetworkManagerFactory::inject();
    auto mock_network_access_manager = std::make_unique<NiceMock<MockQNetworkAccessManager>>();
    auto mock_reply = new MockQNetworkReply();

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
    EXPECT_EQ(send_command({"launch", "--cloud-init", fake_url.toStdString()}), ReturnCode::Ok);
}

TEST_F(Client, launchCmdWithUnixAbsolutePath)
{
    EXPECT_CALL(mock_daemon, launch).WillOnce(Return(ok));
    EXPECT_EQ(send_command({"launch", "file:///this/is/absolute"}), ReturnCode::Ok);
    EXPECT_EQ(send_command({"launch", "file:///this/is/./also/absolute"}), ReturnCode::Ok);
    EXPECT_EQ(send_command({"launch", "file:///this/is/../also/./absolute"}), ReturnCode::Ok);
}

TEST_F(Client, launchCmdWithRelativeAbsolutePath)
{
    EXPECT_CALL(mock_daemon, launch).WillRepeatedly(Return(ok));
    EXPECT_EQ(send_command({"launch", "file://./this/is/relative"}), ReturnCode::Ok);
    EXPECT_EQ(send_command({"launch", "file://../this/is/also/relative"}), ReturnCode::Ok);
    EXPECT_EQ(send_command({"launch", "file://../this/./is/../also/relative"}), ReturnCode::Ok);
}

TEST_F(Client, launchCmdProvidesAccurateLocalPathHint)
{
    auto [mock_file_ops, mocked_file_ops_guard] = MockFileOps::inject();
    ON_CALL(*mock_file_ops, exists(An<const QFileInfo&>())).WillByDefault(Return(true));

    EXPECT_CALL(mock_daemon, launch).Times(0);

    {
        auto cerr_stream = std::stringstream();
        EXPECT_EQ(send_command({"launch", "file://C:/missing/slash"}, trash_stream, cerr_stream),
                  ReturnCode::CommandLineError);
        EXPECT_THAT(cerr_stream.str(),
                    testing::HasSubstr("Did you mean: `file:///C:/missing/slash`?"));
    }

    {
        auto cerr_stream = std::stringstream();
        EXPECT_EQ(send_command({"launch", "file:///C:\\backward/slash"}, trash_stream, cerr_stream),
                  ReturnCode::CommandLineError);
        EXPECT_THAT(cerr_stream.str(),
                    testing::HasSubstr("Did you mean: `file:///C:/backward/slash`?"));
    }

    {
        auto cerr_stream = std::stringstream();
        EXPECT_EQ(send_command({"launch", "file:///C:\\backward/slash"}, trash_stream, cerr_stream),
                  ReturnCode::CommandLineError);
        EXPECT_THAT(cerr_stream.str(),
                    testing::HasSubstr("Did you mean: `file:///C:/backward/slash`?"));
    }

    {
        auto cerr_stream = std::stringstream();
        EXPECT_EQ(send_command({"launch", "file://not/a/local/file"}, trash_stream, cerr_stream),
                  ReturnCode::CommandLineError);
        EXPECT_THAT(cerr_stream.str(),
                    testing::HasSubstr("Did you mean: `file://./not/a/local/file`?"));
    }

    {
        auto cerr_stream = std::stringstream();
        EXPECT_EQ(send_command({"launch", "file://./relative\\with/backslash"}, trash_stream, cerr_stream),
                  ReturnCode::CommandLineError);
        EXPECT_THAT(cerr_stream.str(),
                    testing::HasSubstr("Did you mean: `file://./relative\\with/backslash`?"));
    }

    {
        auto cerr_stream = std::stringstream();
        EXPECT_EQ(send_command({"launch", "file://../relative\\with/backslash"}, trash_stream, cerr_stream),
                  ReturnCode::CommandLineError);
        EXPECT_THAT(cerr_stream.str(),
                    testing::HasSubstr("Did you mean: `file://../relative\\with/backslash`?"));
    }
}

} // namespace multipass::test
