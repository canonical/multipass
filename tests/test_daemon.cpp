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

#include "blueprint_test_lambdas.h"
#include "common.h"
#include "daemon_test_fixture.h"
#include "dummy_ssh_key_provider.h"
#include "fake_alias_config.h"
#include "json_test_utils.h"
#include "mock_daemon.h"
#include "mock_environment_helpers.h"
#include "mock_file_ops.h"
#include "mock_image_host.h"
#include "mock_json_utils.h"
#include "mock_logger.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_standard_paths.h"
#include "mock_utils.h"
#include "mock_virtual_machine.h"
#include "mock_vm_blueprint_provider.h"
#include "mock_vm_image_vault.h"
#include "path.h"
#include "stub_virtual_machine.h"
#include "stub_vm_image_vault.h"
#include "tracking_url_downloader.h"

#include <src/daemon/default_vm_image_vault.h>
#include <src/daemon/instance_settings_handler.h>

#include <multipass/constants.h>
#include <multipass/default_vm_blueprint_provider.h>
#include <multipass/exceptions/blueprint_exceptions.h>
#include <multipass/logging/log.h>
#include <multipass/name_generator.h>
#include <multipass/version.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>

#include <yaml-cpp/yaml.h>

#include <scope_guard.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxyFactory>
#include <QStorageInfo>
#include <QString>
#include <QSysInfo>

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;
using namespace multipass::utils;

namespace YAML
{
void PrintTo(const YAML::Node& node, ::std::ostream* os)
{
    YAML::Emitter emitter;
    emitter.SetIndent(4);
    emitter << node;
    *os << "\n" << emitter.c_str();
}
} // namespace YAML

namespace
{
const qint64 default_total_bytes{16'106'127'360}; // 15G

const std::string csv_header{"Alias,Instance,Command,Working directory,Context\n"};

struct StubNameGenerator : public mp::NameGenerator
{
    explicit StubNameGenerator(std::string name) : name{std::move(name)}
    {
    }
    std::string make_name() override
    {
        return name;
    }
    std::string name;
};
} // namespace

struct Daemon : public mpt::DaemonTestFixture
{
    Daemon()
    {
        ON_CALL(mock_utils, filesystem_bytes_available(_)).WillByDefault([this](const QString& data_directory) {
            return mock_utils.Utils::filesystem_bytes_available(data_directory);
        });

        EXPECT_CALL(mock_platform, get_blueprints_url_override()).WillRepeatedly([] { return QString{}; });
        EXPECT_CALL(mock_platform, multipass_storage_location()).Times(AnyNumber()).WillRepeatedly(Return(QString()));
        EXPECT_CALL(mock_platform, create_alias_script(_, _)).WillRepeatedly(Return());
        EXPECT_CALL(mock_platform, remove_alias_script(_)).WillRepeatedly(Return());
    }

    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::petenv_key))).WillRepeatedly(Return("pet-instance"));
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true")); /* TODO should probably add
                             a few more tests for `false`, since there are different portions of code depending on it */
        EXPECT_CALL(mock_settings, get(Eq(mp::winterm_key))).WillRepeatedly(Return("none"));
        EXPECT_CALL(mock_settings, get(Eq(mp::bridged_interface_key))).WillRepeatedly(Return("eth8"));
    }

    mpt::MockUtils::GuardedMock mock_utils_injection{mpt::MockUtils::inject<NiceMock>()};
    mpt::MockUtils& mock_utils = *mock_utils_injection.first;

    mpt::MockPlatform::GuardedMock mock_platform_injection{mpt::MockPlatform::inject()};
    mpt::MockPlatform& mock_platform = *mock_platform_injection.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(Daemon, receives_commands_and_calls_corresponding_slot)
{
    mpt::MockDaemon daemon{config_builder.build()};

    EXPECT_CALL(daemon, keys)
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::KeysRequest, mp::KeysReply>));
    EXPECT_CALL(daemon, get)
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::GetRequest, mp::GetReply>));
    EXPECT_CALL(daemon, set)
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::SetRequest, mp::SetReply>));
    EXPECT_CALL(daemon, create(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::CreateRequest, mp::CreateReply>));
    EXPECT_CALL(daemon, launch(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::LaunchRequest, mp::LaunchReply>));
    EXPECT_CALL(daemon, purge(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::PurgeRequest, mp::PurgeReply>));
    EXPECT_CALL(daemon, find(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::FindRequest, mp::FindReply>));
    EXPECT_CALL(daemon, ssh_info(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::SSHInfoRequest, mp::SSHInfoReply>));
    EXPECT_CALL(daemon, info(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::InfoRequest, mp::InfoReply>));
    EXPECT_CALL(daemon, list(_, _, _)).WillOnce([](auto, auto server, auto status_promise) {
        mp::ListReply reply;
        reply.mutable_instance_list();

        server->Write(reply);
        status_promise->set_value(grpc::Status::OK);

        return grpc::Status{};
    });
    EXPECT_CALL(daemon, recover(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::RecoverRequest, mp::RecoverReply>));
    EXPECT_CALL(daemon, start(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::StartRequest, mp::StartReply>));
    EXPECT_CALL(daemon, stop(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::StopRequest, mp::StopReply>));
    EXPECT_CALL(daemon, suspend(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::SuspendRequest, mp::SuspendReply>));
    EXPECT_CALL(daemon, restart(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::RestartRequest, mp::RestartReply>));
    EXPECT_CALL(daemon, delet(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::DeleteRequest, mp::DeleteReply>));
    EXPECT_CALL(daemon, version(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::VersionRequest, mp::VersionReply>));
    EXPECT_CALL(daemon, mount(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::MountRequest, mp::MountReply>));
    EXPECT_CALL(daemon, umount(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::UmountRequest, mp::UmountReply>));
    EXPECT_CALL(daemon, networks(_, _, _))
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::NetworksRequest, mp::NetworksReply>));
    EXPECT_CALL(daemon, snapshot)
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::SnapshotRequest, mp::SnapshotReply>));
    EXPECT_CALL(daemon, restore)
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::RestoreRequest, mp::RestoreReply>));
    EXPECT_CALL(daemon, clone)
        .WillOnce(Invoke(&daemon, &mpt::MockDaemon::set_promise_value<mp::CloneRequest, mp::CloneReply>));
    EXPECT_CALL(mock_settings, get(Eq("foo"))).WillRepeatedly(Return("bar"));

    send_commands({{"test_keys"},
                   {"test_get", "foo"},
                   {"test_set", "foo", "bar"},
                   {"test_create", "foo"},
                   {"launch", "foo"},
                   {"delete", "foo"},
                   {"exec", "foo", "--no-map-working-directory", "--", "cmd"},
                   {"info", "foo"},
                   {"list"},
                   {"purge"},
                   {"recover", "foo"},
                   {"snapshot", "foo"},
                   {"start", "foo"},
                   {"stop", "foo"},
                   {"suspend", "foo"},
                   {"restart", "foo"},
                   {"restore", "foo.bar"},
                   {"version"},
                   {"find", "something"},
                   {"mount", ".", "target"},
                   {"umount", "instance"},
                   {"networks"},
                   {"clone", "foo"}});
}

TEST_F(Daemon, provides_version)
{
    mp::Daemon daemon{config_builder.build()};
    StrictMock<mpt::MockServerReaderWriter<mp::VersionReply, mp::VersionRequest>> mock_server;
    EXPECT_CALL(mock_server, Write(Property(&mp::VersionReply::version, StrEq(mp::version_string)), _))
        .WillOnce(Return(true));

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::version, mp::VersionRequest{}, mock_server).ok());
}

TEST_F(Daemon, failed_restart_command_returns_fulfilled_promise)
{
    mp::Daemon daemon{config_builder.build()};

    auto nonexistant_instance = new mp::InstanceNames; // on heap as *Request takes ownership
    nonexistant_instance->add_instance_name("nonexistant");
    mp::RestartRequest request;
    request.set_allocated_instance_names(nonexistant_instance);
    std::promise<grpc::Status> status_promise;

    daemon.restart(&request, nullptr, &status_promise);
    EXPECT_TRUE(is_ready(status_promise.get_future()));
}

TEST_F(Daemon, proxy_contains_valid_info)
{
    auto guard = sg::make_scope_guard([]() noexcept {          // std::terminate ok if this throws
        QNetworkProxyFactory::setUseSystemConfiguration(true); // Resets proxy back to what the system is configured for
    });

    QString username{"username"};
    QString password{"password"};
    QString hostname{"192.168.1.1"};
    qint16 port{3128};
    QString proxy = QString("%1:%2@%3:%4").arg(username).arg(password).arg(hostname).arg(port);

    mpt::SetEnvScope env("http_proxy", proxy.toUtf8());

    auto config = config_builder.build();

    EXPECT_THAT(config->network_proxy->user(), username);
    EXPECT_THAT(config->network_proxy->password(), password);
    EXPECT_THAT(config->network_proxy->hostName(), hostname);
    EXPECT_THAT(config->network_proxy->port(), port);
}

TEST_F(Daemon, daemonAppliesPermissionsToStorageDirectory)
{
    QTemporaryDir storage_dir;

    mpt::SetEnvScope storage(mp::multipass_storage_env_var, storage_dir.path().toUtf8());

    EXPECT_CALL(mock_platform, multipass_storage_location()).WillOnce(Return(mp::utils::get_multipass_storage()));
    EXPECT_CALL(mock_utils, make_dir(_, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    auto config = config_builder.build();
}

TEST_F(Daemon, data_path_valid)
{
    QTemporaryDir data_dir, cache_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::CacheLocation))
        .WillOnce(Return(cache_dir.path()));
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::AppDataLocation))
        .WillOnce(Return(data_dir.path()));

    EXPECT_CALL(mock_platform, multipass_storage_location()).WillOnce(Return(QString()));

    config_builder.data_directory = "";
    config_builder.cache_directory = "";
    auto config = config_builder.build();

    EXPECT_EQ(config->data_directory, data_dir.path());
    EXPECT_EQ(config->cache_directory, cache_dir.path());
}

TEST_F(Daemon, data_path_with_storage_valid)
{
    QTemporaryDir storage_dir;

    mpt::SetEnvScope storage(mp::multipass_storage_env_var, storage_dir.path().toUtf8());
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_)).Times(0);

    EXPECT_CALL(mock_platform, multipass_storage_location()).WillOnce(Return(mp::utils::get_multipass_storage()));

    ON_CALL(mock_utils, make_dir(_, _, _))
        .WillByDefault([this](const QDir& a_dir, const QString& name, QFileDevice::Permissions permissions) {
            return mock_utils.Utils::make_dir(a_dir, name, permissions);
        });

    config_builder.data_directory = "";
    config_builder.cache_directory = "";
    auto config = config_builder.build();

    EXPECT_EQ(config->data_directory, storage_dir.filePath("data"));
    EXPECT_EQ(config->cache_directory, storage_dir.filePath("cache"));
}

TEST_F(Daemon, blueprintsDownloadsFromCorrectURL)
{
    mpt::TempDir cache_dir;
    auto url_downloader = std::make_unique<mpt::TrackingURLDownloader>();

    config_builder.cache_directory = cache_dir.path();
    config_builder.url_downloader = std::move(url_downloader);
    config_builder.blueprint_provider.reset();

    auto config = config_builder.build();

    mpt::TrackingURLDownloader* downloader = static_cast<mpt::TrackingURLDownloader*>(config->url_downloader.get());
    EXPECT_EQ(downloader->downloaded_urls.size(), 1);
    EXPECT_EQ(downloader->downloaded_urls.front(), mp::default_blueprint_url);
}

TEST_F(Daemon, blueprintsURLOverrideIsCorrect)
{
    mpt::TempDir cache_dir;
    auto url_downloader = std::make_unique<mpt::TrackingURLDownloader>();
    const QString test_blueprints_zip{mpt::test_data_path() + "test-blueprints.zip"};

    EXPECT_CALL(mock_platform, get_blueprints_url_override()).WillOnce([&test_blueprints_zip] {
        return QUrl::fromLocalFile(test_blueprints_zip).toEncoded();
    });

    config_builder.cache_directory = cache_dir.path();
    config_builder.url_downloader = std::move(url_downloader);
    config_builder.blueprint_provider.reset();

    auto config = config_builder.build();

    mpt::TrackingURLDownloader* downloader = static_cast<mpt::TrackingURLDownloader*>(config->url_downloader.get());
    EXPECT_EQ(downloader->downloaded_urls.size(), 1);
    EXPECT_EQ(downloader->downloaded_urls.front(), QUrl::fromLocalFile(test_blueprints_zip).toString());
}

namespace
{
struct DaemonCreateLaunchTestSuite : public Daemon, public WithParamInterface<std::string>
{
};

struct DaemonCreateLaunchPollinateDataTestSuite : public Daemon,
                                                  public WithParamInterface<std::tuple<std::string, std::string>>
{
};

struct DaemonCreateLaunchAliasTestSuite : public Daemon, public FakeAliasConfig, public WithParamInterface<std::string>
{
    DaemonCreateLaunchAliasTestSuite()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(fake_alias_dir.path()));
    }
};

struct MinSpaceRespectedSuite : public Daemon,
                                public WithParamInterface<std::tuple<std::string, std::string, std::string>>
{
};

struct MinSpaceViolatedSuite : public Daemon,
                               public WithParamInterface<std::tuple<std::string, std::string, std::string>>
{
};

struct LaunchImgSizeSuite : public Daemon,
                            public WithParamInterface<std::tuple<std::string, std::vector<std::string>, std::string>>
{
};

struct LaunchStorageCheckSuite : public Daemon, public WithParamInterface<std::string>
{
};

struct LaunchWithBridges
    : public Daemon,
      public WithParamInterface<
          std::pair<std::vector<std::tuple<std::string, std::string, std::string>>, std::vector<std::string>>>
{
};

struct LaunchWithNoNetworkCloudInit : public Daemon, public WithParamInterface<std::vector<std::string>>
{
};

struct RefuseBridging : public Daemon, public WithParamInterface<std::tuple<std::string, std::string>>
{
};

struct ListIP : public Daemon,
                public WithParamInterface<
                    std::tuple<mp::VirtualMachine::State, std::vector<std::string>, std::vector<std::string>>>
{
};

struct DaemonLaunchTimeoutValueTestSuite : public Daemon, public WithParamInterface<std::tuple<int, int, int>>
{
};

TEST_P(DaemonCreateLaunchTestSuite, creates_virtual_machines)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine);
    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchTestSuite, on_creation_hooks_up_platform_prepare_source_image)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_source_image(_));
    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchTestSuite, on_creation_hooks_up_platform_prepare_instance_image)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _));
    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchTestSuite, on_creation_handles_instance_image_preparation_failure)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    std::string cause = "motive";
    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _)).WillOnce(Throw(std::runtime_error{cause}));
    EXPECT_CALL(*mock_factory, remove_resources_for(_));

    std::stringstream err_stream;
    send_command({GetParam()}, trash_stream, err_stream);

    EXPECT_THAT(err_stream.str(), AllOf(HasSubstr("failed"), HasSubstr(cause)));
}

TEST_P(DaemonCreateLaunchTestSuite, generates_name_on_creation_when_client_does_not_provide_one)
{
    const std::string expected_name{"pied-piper-valley"};

    config_builder.name_generator = std::make_unique<StubNameGenerator>(expected_name);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({GetParam()}, stream);

    EXPECT_THAT(stream.str(), HasSubstr(expected_name));
}

MATCHER_P2(YAMLNodeContainsString, key, val, "")
{
    if (!arg.IsMap())
    {
        return false;
    }
    if (!arg[key])
    {
        return false;
    }
    if (!arg[key].IsScalar())
    {
        return false;
    }
    return arg[key].Scalar() == val;
}

MATCHER_P2(YAMLNodeContainsStringStartingWith, key, val, "")
{
    if (!arg.IsMap())
    {
        return false;
    }
    if (!arg[key])
    {
        return false;
    }
    if (!arg[key].IsScalar())
    {
        return false;
    }
    return arg[key].Scalar().find(val) == 0;
}

MATCHER_P(YAMLNodeContainsSubString, val, "")
{
    if (!arg.IsSequence())
    {
        return false;
    }

    return arg[0].Scalar().find(val) != std::string::npos;
}

MATCHER_P2(YAMLNodeContainsStringArray, key, values, "")
{
    if (!arg.IsMap())
    {
        return false;
    }
    if (!arg[key])
    {
        return false;
    }
    if (!arg[key].IsSequence())
    {
        return false;
    }
    if (arg[key].size() != values.size())
    {
        return false;
    }
    for (auto i = 0u; i < values.size(); ++i)
    {
        if (arg[key][i].template as<std::string>() != values[i])
        {
            return false;
        }
    }
    return true;
}

MATCHER_P(YAMLNodeContainsMap, key, "")
{
    if (!arg.IsMap())
    {
        return false;
    }
    if (!arg[key])
    {
        return false;
    }
    return arg[key].IsMap();
}

MATCHER_P(YAMLNodeContainsSequence, key, "")
{
    if (!arg.IsMap())
    {
        return false;
    }
    if (!arg[key])
    {
        return false;
    }
    return arg[key].IsSequence();
}

MATCHER_P(YAMLSequenceContainsStringMap, values, "")
{
    if (!arg.IsSequence())
    {
        return false;
    }
    for (const auto& node : arg)
    {
        if (node.size() != values.size())
            continue;
        for (auto it = values.cbegin();; ++it)
        {
            if (it == values.cend())
                return true;
            else if (node[it->first].template as<std::string>() != it->second)
                break;
        }
    }
    return false;
}

TEST_P(DaemonCreateLaunchTestSuite, default_cloud_init_grows_root_fs)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _))
        .WillOnce(Invoke([](const multipass::VMImage&, const mp::VirtualMachineDescription& desc) {
            EXPECT_THAT(desc.vendor_data_config, YAMLNodeContainsMap("growpart"));

            if (desc.vendor_data_config["growpart"])
            {
                auto const& growpart_stanza = desc.vendor_data_config["growpart"];

                EXPECT_THAT(growpart_stanza, YAMLNodeContainsString("mode", "auto"));
                EXPECT_THAT(growpart_stanza, YAMLNodeContainsStringArray("devices", std::vector<std::string>({"/"})));
                EXPECT_THAT(growpart_stanza, YAMLNodeContainsString("ignore_growroot_disabled", "false"));
            }
        }));

    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchTestSuite, adds_ssh_keys_to_cloud_init_config)
{
    auto mock_factory = use_a_mock_vm_factory();
    std::string expected_key{"thisitnotansshkeyactually"};
    config_builder.ssh_key_provider = std::make_unique<mpt::DummyKeyProvider>(expected_key);
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _))
        .WillOnce(Invoke([&expected_key](const multipass::VMImage&, const mp::VirtualMachineDescription& desc) {
            ASSERT_THAT(desc.vendor_data_config, YAMLNodeContainsSequence("ssh_authorized_keys"));
            auto const& ssh_keys_stanza = desc.vendor_data_config["ssh_authorized_keys"];
            EXPECT_THAT(ssh_keys_stanza, YAMLNodeContainsSubString(expected_key));
        }));

    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchPollinateDataTestSuite, adds_pollinate_user_agent_to_cloud_init_config)
{
    const auto [command, alias] = GetParam();
    auto mock_factory = use_a_mock_vm_factory();
    std::vector<std::pair<std::string, std::string>> const& expected_pollinate_map{
        {"path", "/etc/pollinate/add-user-agent"},
        {"content", fmt::format("multipass/version/{} # written by Multipass\n"
                                "multipass/driver/mock-1234 # written by Multipass\n"
                                "multipass/host/{}-{} # written by Multipass\n"
                                "multipass/alias/{} # written by Multipass\n",
                                multipass::version_string, QSysInfo::productType(), QSysInfo::productVersion(),
                                alias.empty() ? "default" : alias)}};
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _))
        .WillOnce(
            Invoke([&expected_pollinate_map](const multipass::VMImage&, const mp::VirtualMachineDescription& desc) {
                EXPECT_THAT(desc.vendor_data_config, YAMLNodeContainsSequence("write_files"));

                if (desc.vendor_data_config["write_files"])
                {
                    auto const& write_stanza = desc.vendor_data_config["write_files"];

                    EXPECT_THAT(write_stanza, YAMLSequenceContainsStringMap(expected_pollinate_map));
                }
            }));

    send_command({command, alias});
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundPassesExpectedAliases)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};
    const std::string alias_name{"aliasname"};
    const std::string alias_command{"aliascommand"};
    const std::string alias_wdir{"map"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    auto alias = std::make_optional(std::make_pair(alias_name, mp::AliasDefinition{name, alias_command, alias_wdir}));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, alias));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    send_command({"launch"});

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    auto expected_csv_string =
        csv_header + alias_name + "," + name + "," + alias_command + "," + alias_wdir + "," + name + "\n";
    EXPECT_EQ(cout_stream.str(), expected_csv_string);
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundMountsWorkspace)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(
            mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt, name));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream;
    send_command({"launch"}, cout_stream);
    EXPECT_THAT(cout_stream.str(),
                AllOf(HasSubstr("Mounted '"), HasSubstr(name + "' into '" + name + ":" + name + "'")));
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundMountsWorkspaceConfined)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(
            mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt, name));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    mpt::TempDir temp_dir;
    mpt::SetEnvScope env_scope1("SNAP_NAME", "multipass");
    mpt::SetEnvScope env_scope2("SNAP_REAL_HOME", temp_dir.path().toUtf8());

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream;
    send_command({"launch"}, cout_stream);
    EXPECT_THAT(cout_stream.str(),
                AllOf(HasSubstr("Mounted '"), HasSubstr(name + "' into '" + name + ":" + name + "'")));
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundMountsWorkspaceInExistingDir)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(
            mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt, name));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    mpt::TempDir temp_dir;
    auto workspace_path = temp_dir.path() + "/multipass/" + QString::fromStdString(name);
    QDir().mkpath(workspace_path);

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(temp_dir.path()));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream, cerr_stream;
    send_command({"launch"}, cout_stream, cerr_stream);

    EXPECT_THAT(cerr_stream.str(), HasSubstr(fmt::format("Folder \"{}\" already exists.", workspace_path)));
    EXPECT_THAT(cout_stream.str(),
                AllOf(HasSubstr("Mounted '"), HasSubstr(name + "' into '" + name + ":" + name + "'")));
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundDoesNotMountUnwrittableWorkspace)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(
            mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt, name));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    mpt::TempDir temp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(temp_dir.path()));

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillRepeatedly(Return(1234));
    EXPECT_CALL(*mock_file_ops, commit(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true)).WillOnce(Return(true)).WillOnce(Return(false));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream, cerr_stream;
    send_command({"launch"}, cout_stream, cerr_stream);

    EXPECT_THAT(cerr_stream.str(), HasSubstr(fmt::format("Error creating folder {}/multipass/{}. Not mounting.\n",
                                                         temp_dir.path(), name)));
    EXPECT_THAT(cout_stream.str(), Not(HasSubstr("Mounted")));
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundButCannotMount)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(
            mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt, name));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    mpt::TempDir temp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::HomeLocation))
        .WillOnce(Return(temp_dir.path()));

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _)).WillRepeatedly(Return(1234));
    EXPECT_CALL(*mock_file_ops, commit(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true)).WillOnce(Return(true)).WillOnce(Return(true));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream, cerr_stream;
    send_command({"launch"}, cout_stream, cerr_stream);

    EXPECT_THAT(cerr_stream.str(),
                HasSubstr(fmt::format("Source path \"{}/multipass/{}\" does not exist", temp_dir.path(), name)));
    EXPECT_THAT(cout_stream.str(), Not(HasSubstr("Mounted")));
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundPassesExpectedAliasesWithNameOverride)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};
    const std::string command_line_name{"name-override"};
    const std::string alias_name{"aliasname"};
    const std::string alias_command{"aliascommand"};
    const std::string alias_wdir{"map"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, command_line_name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    auto alias = std::make_optional(std::make_pair(alias_name, mp::AliasDefinition{name, alias_command, alias_wdir}));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, alias));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(command_line_name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    send_command({"launch", name, "-n", command_line_name});

    std::stringstream cout_stream;
    send_command({"aliases", "--format=csv"}, cout_stream);

    auto expected_csv_string = csv_header + alias_name + "," + command_line_name + "," + alias_command + "," +
                               alias_wdir + "," + command_line_name + "\n";
    EXPECT_EQ(cout_stream.str(), expected_csv_string);
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundDoesNotOverwriteAliases)
{
    typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};
    const std::string alias_name{"aliasname"};
    const std::string alias_command{"aliascommand"};
    const std::string alias_wdir{"map"};

    // This makes the alias be defined in the default context. Launching an instance will define it in another context.
    populate_db_file(AliasesVector{{alias_name, {"original_instance", "a_command", "map"}}});

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    auto alias = std::make_optional(std::make_pair(alias_name, mp::AliasDefinition{name, alias_command, alias_wdir}));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, alias));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream;
    send_command({"launch", name}, cout_stream);

    cout_stream.str("");
    send_command({"aliases", "--format=csv"}, cout_stream);

    auto expected_csv_string = csv_header + alias_name + ",original_instance,a_command,map,default*\n" + alias_name +
                               "," + name + "," + alias_command + "," + alias_wdir + "," + name + "\n";
    EXPECT_EQ(cout_stream.str(), expected_csv_string);
}

TEST_F(DaemonCreateLaunchAliasTestSuite, blueprintFoundDoesNotOverwriteAliasesIfClashing)
{
    typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string alias_name{"aliasname"};
    const std::string alias_command{"aliascommand"};
    const std::string alias_wdir{"map"};

    // The easiest way of creating a clash is to name the instance like the default alias context.
    const std::string name{"default"};

    populate_db_file(AliasesVector{{alias_name, {"original_instance", "a_command", "map"}}});

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    auto alias = std::make_optional(std::make_pair(alias_name, mp::AliasDefinition{name, alias_command, alias_wdir}));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, alias));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream cout_stream, cerr_stream;
    send_command({"launch", name}, cout_stream, cerr_stream);

    EXPECT_THAT(cerr_stream.str(), HasSubstr("Warning: unable to create alias " + alias_name));

    cout_stream.str("");
    send_command({"aliases", "--format=csv"}, cout_stream);

    auto expected_csv_string = csv_header + alias_name + ",original_instance,a_command,map,default*\n";
    EXPECT_EQ(cout_stream.str(), expected_csv_string);
}

TEST_P(DaemonCreateLaunchTestSuite, blueprint_found_passes_expected_data)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    static constexpr int num_cores = 4;
    const mp::MemorySize mem_size{"4G"};
    const mp::MemorySize disk_space{"25G"};
    const std::string release{"focal"};
    const std::string remote{"release"};
    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchTestSuite, blueprint_not_found_passes_expected_data)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    static constexpr int num_cores = 1;
    const mp::MemorySize mem_size{"1G"};
    const mp::MemorySize disk_space{"5G"};
    const std::string empty{};
    const std::string release{"default"};

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, empty));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, empty));

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    send_command({GetParam()});
}

TEST_P(LaunchWithNoNetworkCloudInit, no_network_cloud_init)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    const auto launch_args = GetParam();

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _))
        .WillOnce(Invoke([](const multipass::VMImage&, const mp::VirtualMachineDescription& desc) {
            EXPECT_TRUE(desc.network_data_config.IsNull());
        }));

    send_command(launch_args);
}

std::vector<std::string> make_args(const std::vector<std::string>& args)
{
    std::vector<std::string> all_args{"launch"};

    for (const auto& a : args)
        all_args.push_back(a);

    return all_args;
}

INSTANTIATE_TEST_SUITE_P(
    Daemon, LaunchWithNoNetworkCloudInit,
    Values(make_args({}), make_args({"xenial"}), make_args({"xenial", "--network", "name=eth0,mode=manual"}),
           make_args({"groovy"}), make_args({"groovy", "--network", "name=eth0,mode=manual"}),
           make_args({"--network", "name=eth0,mode=manual"}), make_args({"devel"}),
           make_args({"hirsute", "--network", "name=eth0,mode=manual"}), make_args({"daily:21.04"}),
           make_args({"daily:21.04", "--network", "name=eth0,mode=manual"}),
           make_args({"appliance:openhab", "--network", "name=eth0,mode=manual"}), make_args({"appliance:nextcloud"}),
           make_args({"snapcraft:core18", "--network", "name=eth0,mode=manual"}), make_args({"snapcraft:core20"})));

TEST_P(LaunchWithBridges, creates_network_cloud_init_iso)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    const auto test_params = GetParam();
    const auto args = test_params.first;
    const auto forbidden_names = test_params.second;

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _))
        .WillOnce(Invoke([&args, &forbidden_names](const multipass::VMImage&,
                                                   const mp::VirtualMachineDescription& desc) {
            EXPECT_THAT(desc.network_data_config, YAMLNodeContainsMap("ethernets"));

            EXPECT_THAT(desc.network_data_config["ethernets"], YAMLNodeContainsMap("default"));
            auto const& default_network_stanza = desc.network_data_config["ethernets"]["default"];
            EXPECT_THAT(default_network_stanza, YAMLNodeContainsMap("match"));
            EXPECT_THAT(default_network_stanza["match"], YAMLNodeContainsStringStartingWith("macaddress", "52:54:00:"));
            EXPECT_THAT(default_network_stanza, YAMLNodeContainsString("dhcp4", "true"));

            for (const auto& arg : args)
            {
                std::string name = std::get<1>(arg);
                if (!name.empty())
                {
                    EXPECT_THAT(desc.network_data_config["ethernets"], YAMLNodeContainsMap(name));
                    auto const& extra_stanza = desc.network_data_config["ethernets"][name];
                    EXPECT_THAT(extra_stanza, YAMLNodeContainsMap("match"));

                    std::string mac = std::get<2>(arg);
                    if (mac.size() == 17)
                        EXPECT_THAT(extra_stanza["match"], YAMLNodeContainsString("macaddress", mac));
                    else
                        EXPECT_THAT(extra_stanza["match"], YAMLNodeContainsStringStartingWith("macaddress", mac));

                    EXPECT_THAT(extra_stanza, YAMLNodeContainsString("dhcp4", "true"));
                    EXPECT_THAT(extra_stanza, YAMLNodeContainsMap("dhcp4-overrides"));
                    EXPECT_THAT(extra_stanza["dhcp4-overrides"], YAMLNodeContainsString("route-metric", "200"));
                    EXPECT_THAT(extra_stanza, YAMLNodeContainsString("optional", "true"));
                }
            }

            for (const auto& forbidden : forbidden_names)
            {
                EXPECT_THAT(desc.network_data_config["ethernets"], Not(YAMLNodeContainsMap(forbidden)));
            }
        }));

    std::vector<std::string> command(1, "launch");

    for (const auto& arg : args)
    {
        command.push_back("--network");
        command.push_back(std::get<0>(arg));
    }

    send_command(command);
}

typedef typename std::pair<std::vector<std::tuple<std::string, std::string, std::string>>, std::vector<std::string>>
    BridgeTestArgType;

INSTANTIATE_TEST_SUITE_P(
    Daemon, LaunchWithBridges,
    Values(BridgeTestArgType({{"eth0", "extra0", "52:54:00:"}}, {"extra1"}),
           BridgeTestArgType({{"name=eth0,mac=01:23:45:ab:cd:ef,mode=auto", "extra0", "01:23:45:ab:cd:ef"},
                              {"wlan0", "extra1", "52:54:00:"}},
                             {"extra2"}),
           BridgeTestArgType({{"name=eth0,mode=manual", "", ""}, {"name=wlan0", "extra1", "52:54:00:"}},
                             {"extra0", "extra2"})));

TEST_P(MinSpaceRespectedSuite, accepts_launch_with_enough_explicit_memory)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    const auto param = GetParam();
    const auto& cmd = std::get<0>(param);
    const auto& opt_name = std::get<1>(param);
    const auto& opt_value = std::get<2>(param);

    EXPECT_CALL(*mock_factory, create_virtual_machine);
    send_command({cmd, opt_name, opt_value});
}

TEST_P(MinSpaceViolatedSuite, refuses_launch_with_memory_below_threshold)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    const auto param = GetParam();
    const auto& cmd = std::get<0>(param);
    const auto& opt_name = std::get<1>(param);
    const auto& opt_value = std::get<2>(param);

    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(0); // expect *no* call
    send_command({cmd, opt_name, opt_value}, trash_stream, stream);
    EXPECT_THAT(stream.str(), AllOf(HasSubstr("fail"), AnyOf(HasSubstr("memory"), HasSubstr("disk"))));
}

TEST_P(LaunchImgSizeSuite, launches_with_correct_disk_size)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto param = GetParam();
    const auto& first_command_line_parameter = std::get<0>(param);
    const auto& other_command_line_parameters = std::get<1>(param);
    const auto& img_size_str = std::get<2>(param);
    const auto img_size = mp::MemorySize(img_size_str);

    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    ON_CALL(*mock_image_vault.get(), minimum_image_size_for(_)).WillByDefault([&img_size_str](auto...) {
        return mp::MemorySize{img_size_str};
    });

    EXPECT_CALL(mock_utils, filesystem_bytes_available(_)).WillRepeatedly(Return(default_total_bytes));

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::vector<std::string> all_parameters{first_command_line_parameter};
    for (const auto& p : other_command_line_parameters)
        all_parameters.push_back(p);

    if (other_command_line_parameters.size() > 0 && mp::MemorySize(other_command_line_parameters[1]) < img_size)
    {
        std::stringstream stream;
        EXPECT_CALL(*mock_factory, create_virtual_machine).Times(0);
        send_command(all_parameters, trash_stream, stream);
        EXPECT_THAT(stream.str(), AllOf(HasSubstr("Requested disk"), HasSubstr("below minimum for this image")));
    }
    else
    {
        EXPECT_CALL(*mock_factory, create_virtual_machine);
        send_command(all_parameters);
    }
}

TEST_P(LaunchStorageCheckSuite, launch_warns_when_overcommitting_disk)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_utils, filesystem_bytes_available(_)).WillRepeatedly(Return(0));

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error, "autostart prerequisites", AtMost(1));
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         fmt::format("Reserving more disk space ({} bytes) than available (0 bytes)",
                                                     mp::MemorySize{mp::default_disk_size}.in_bytes()));
    EXPECT_CALL(*mock_factory, create_virtual_machine);
    send_command({GetParam()});
}

TEST_P(LaunchStorageCheckSuite, launch_fails_when_space_less_than_image)
{
    auto mock_factory = use_a_mock_vm_factory();

    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    ON_CALL(*mock_image_vault.get(), minimum_image_size_for(_)).WillByDefault([](auto...) {
        return mp::MemorySize{"1"};
    });
    config_builder.vault = std::move(mock_image_vault);

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_utils, filesystem_bytes_available(_)).WillRepeatedly(Return(0));

    std::stringstream stream;
    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(0);
    send_command({GetParam()}, trash_stream, stream);
    EXPECT_THAT(stream.str(), HasSubstr("Available disk (0 bytes) below minimum for this image (1 bytes)"));
}

TEST_P(LaunchStorageCheckSuite, launch_fails_with_invalid_data_directory)
{
    auto mock_factory = use_a_mock_vm_factory();
    config_builder.data_directory = QString("invalid_data_directory");
    mp::Daemon daemon{config_builder.build()};

    auto [mock_json_utils, guard] = mpt::MockJsonUtils::inject<StrictMock>();
    EXPECT_CALL(*mock_json_utils, write_json).Times(1); // avoid creating directory

    std::stringstream stream;
    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(0);
    send_command({GetParam()}, trash_stream, stream);
    EXPECT_THAT(stream.str(), HasSubstr("Failed to determine information about the volume containing"));
}

INSTANTIATE_TEST_SUITE_P(Daemon, DaemonCreateLaunchTestSuite, Values("launch", "test_create"));
INSTANTIATE_TEST_SUITE_P(Daemon, DaemonCreateLaunchPollinateDataTestSuite,
                         Combine(Values("launch", "test_create"), Values("foo", "")));
INSTANTIATE_TEST_SUITE_P(Daemon, MinSpaceRespectedSuite,
                         Combine(Values("test_create", "launch"), Values("--memory", "--disk"),
                                 Values("1024m", "2Gb", "987654321")));
INSTANTIATE_TEST_SUITE_P(Daemon, MinSpaceViolatedSuite,
                         Combine(Values("test_create", "launch"), Values("--memory", "--disk"),
                                 Values("0", "0B", "0GB", "123B", "42kb", "100")));
INSTANTIATE_TEST_SUITE_P(Daemon, LaunchImgSizeSuite,
                         Combine(Values("test_create", "launch"),
                                 Values(std::vector<std::string>{}, std::vector<std::string>{"--disk", "4G"}),
                                 Values("1G", mp::default_disk_size, "10G")));
INSTANTIATE_TEST_SUITE_P(Daemon, LaunchStorageCheckSuite, Values("test_create", "launch"));

TEST_F(DaemonCreateLaunchTestSuite, blueprintFromFileCallsCorrectFunction)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    const std::string name{"ultimo-blueprint"};

    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(1);

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).Times(1);

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _)).Times(0);

    EXPECT_CALL(*mock_blueprint_provider, blueprint_from_file(_, _, _, _)).Times(1);

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    send_command({"launch", "file://blah.yaml"});
}

void check_maps_in_json(const QString& file, const mp::id_mappings& expected_gid_mappings,
                        const mp::id_mappings& expected_uid_mappings)
{
    QByteArray json = mpt::load(file);

    QJsonParseError parse_error;
    const auto doc = QJsonDocument::fromJson(json, &parse_error);
    EXPECT_FALSE(doc.isNull());
    EXPECT_TRUE(doc.isObject());

    const auto doc_object = doc.object();
    const auto instance_object = doc_object["real-zebraphant"].toObject();

    const auto mounts = instance_object["mounts"].toArray();

    ASSERT_EQ(mounts.size(), 1);

    auto mount = mounts.first().toObject(); // There is at most one mount in our JSON.

    auto gid_mappings = mount["gid_mappings"].toArray();

    ASSERT_EQ((unsigned)gid_mappings.size(), expected_gid_mappings.size());

    for (unsigned i = 0; i < expected_gid_mappings.size(); ++i)
    {
        ASSERT_EQ(gid_mappings[i].toObject()["host_gid"], expected_gid_mappings[i].first);
        ASSERT_EQ(gid_mappings[i].toObject()["instance_gid"], expected_gid_mappings[i].second);
    }

    auto uid_mappings = mount["uid_mappings"].toArray();
    ASSERT_EQ((unsigned)uid_mappings.size(), expected_uid_mappings.size());

    for (unsigned i = 0; i < expected_uid_mappings.size(); ++i)
    {
        ASSERT_EQ(uid_mappings[i].toObject()["host_uid"], expected_uid_mappings[i].first);
        ASSERT_EQ(uid_mappings[i].toObject()["instance_uid"], expected_uid_mappings[i].second);
    }
}

TEST_F(Daemon, reads_mac_addresses_from_json)
{
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    std::string mac_addr("52:54:00:73:76:28");
    std::vector<mp::NetworkInterface> extra_interfaces{
        mp::NetworkInterface{"wlx60e3270f55fe", "52:54:00:bd:19:41", true},
        mp::NetworkInterface{"enp3s0", "01:23:45:67:89:ab", false}};

    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    EXPECT_CALL(*use_a_mock_vm_factory(), create_virtual_machine).WillRepeatedly(WithArg<0>([](const auto& desc) {
        return std::make_unique<mpt::StubVirtualMachine>(desc.vm_name);
    }));

    // Make the daemon look for the JSON on our temporary directory. It will read the contents of the file.
    config_builder.data_directory = temp_dir->path();
    mp::Daemon daemon{config_builder.build()};

    // Check that the instance was indeed read and there were no errors.
    {
        StrictMock<mpt::MockServerReaderWriter<mp::ListReply, mp::ListRequest>> mock_server;
        mp::ListReply list_reply;

        auto instance_matcher = UnorderedElementsAre(Property(&mp::ListVMInstance::name, "real-zebraphant"));
        EXPECT_CALL(mock_server, Write(_, _)).WillOnce(DoAll(SaveArg<0>(&list_reply), Return(true)));

        EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::list, mp::ListRequest{}, mock_server).ok());
        EXPECT_THAT(list_reply.instance_list().instances(), instance_matcher);
    }

    // Removing the JSON is possible now because data was already read. This step is not necessary, but doing it we
    // make sure that the file was indeed rewritten after the next step.
    QFile::remove(filename);
    daemon.persist_instances();

    // Finally, check the contents of the file. If they match with what we read, we are done.
    check_interfaces_in_json(filename, mac_addr, extra_interfaces);
}

TEST_F(Daemon, writesAndReadsMountsInJson)
{
    ON_CALL(mock_utils, make_dir(_, _, _))
        .WillByDefault([this](const QDir& a_dir, const QString& name, QFileDevice::Permissions permissions) {
            return mock_utils.Utils::make_dir(a_dir, name, permissions);
        });

    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    std::string mac_addr("52:54:00:23:11:97");
    std::vector<mp::NetworkInterface> extra_interfaces{};

    // Create a temp folder containing three subfolders, and create mount points for all of them.
    auto temp_mount_dir = QDir(mpt::TempDir().path());
    auto temp_mount_1 = MP_UTILS.make_dir(temp_mount_dir, QString("a"), QFileDevice::Permissions{}).toStdString();
    auto temp_mount_2 = MP_UTILS.make_dir(temp_mount_dir, QString("b"), QFileDevice::Permissions{}).toStdString();
    auto temp_mount_3 = MP_UTILS.make_dir(temp_mount_dir, QString("c"), QFileDevice::Permissions{}).toStdString();

    mp::id_mappings uid_mappings_1{{123, 321}};
    mp::id_mappings gid_mappings_1{{456, 654}};

    mp::id_mappings uid_mappings_2{{234, 432}};
    mp::id_mappings gid_mappings_2{{567, 765}};

    mp::id_mappings uid_mappings_3{{345, 543}};
    mp::id_mappings gid_mappings_3{{678, 876}};

    std::unordered_map<std::string, mp::VMMount> mounts;

    mounts.emplace("target_1",
                   mp::VMMount{temp_mount_1, uid_mappings_1, gid_mappings_1, mp::VMMount::MountType::Classic});
    mounts.emplace("target_2",
                   mp::VMMount{temp_mount_2, uid_mappings_2, gid_mappings_2, mp::VMMount::MountType::Classic});
    mounts.emplace("target_3",
                   mp::VMMount{temp_mount_3, uid_mappings_3, gid_mappings_3, mp::VMMount::MountType::Classic});

    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces, mounts));

    EXPECT_CALL(*use_a_mock_vm_factory(), create_virtual_machine).WillRepeatedly(WithArg<0>([](const auto& desc) {
        return std::make_unique<mpt::StubVirtualMachine>(desc.vm_name);
    }));

    // Make the daemon look for the JSON on our temporary directory. It will read the contents of the file.
    config_builder.data_directory = temp_dir->path();
    mp::Daemon daemon{config_builder.build()};

    // Check that the instance was indeed read and there were no errors.
    {
        StrictMock<mpt::MockServerReaderWriter<mp::ListReply, mp::ListRequest>> mock_server;
        mp::ListReply list_reply;

        auto instance_matcher = UnorderedElementsAre(Property(&mp::ListVMInstance::name, "real-zebraphant"));
        EXPECT_CALL(mock_server, Write(_, _)).WillOnce(DoAll(SaveArg<0>(&list_reply), Return(true)));

        EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::list, mp::ListRequest{}, mock_server).ok());
        EXPECT_THAT(list_reply.instance_list().instances(), instance_matcher);
    }

    QFile::remove(filename);    // Remove the JSON.
    daemon.persist_instances(); // Write it again to disk.
    check_mounts_in_json(filename, mounts);
}

TEST_F(Daemon, writes_and_reads_ordered_maps_in_json)
{
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::id_mappings uid_mappings{{1002, 0}, {1001, 1}};
    mp::id_mappings gid_mappings{{1002, 0}, {1000, 2}};
    std::unordered_map<std::string, mp::VMMount> mounts;
    mpt::TempDir dir;
    mounts.emplace("Home",
                   mp::VMMount{dir.path().toStdString(), uid_mappings, gid_mappings, mp::VMMount::MountType::Classic});

    const auto [temp_dir, filename] =
        plant_instance_json(fake_json_contents("52:54:00:73:76:29", std::vector<mp::NetworkInterface>{}, mounts));

    EXPECT_CALL(*use_a_mock_vm_factory(), create_virtual_machine).WillRepeatedly(WithArg<0>([](const auto& desc) {
        return std::make_unique<mpt::StubVirtualMachine>(desc.vm_name);
    }));

    config_builder.data_directory = temp_dir->path();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"list"}, stream);
    EXPECT_THAT(stream.str(), HasSubstr("real-zebraphant"));

    QFile::remove(filename);

    send_command({"purge"});

    check_maps_in_json(filename, uid_mappings, gid_mappings);
}

TEST_F(Daemon, launches_with_valid_network_interface)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, networks());

    ASSERT_NO_THROW(send_command({"launch", "--network", "eth0"}));
}

TEST_F(Daemon, refuses_launch_with_invalid_network_interface)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, networks());

    std::stringstream err_stream;
    send_command({"launch", "--network", "eth2"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(), HasSubstr("Invalid network options supplied"));
}

TEST_F(Daemon, refuses_launch_because_bridging_is_not_implemented)
{
    // Use the stub factory, which throws when networks() is called.
    mp::Daemon daemon{config_builder.build()};

    std::stringstream err_stream;
    send_command({"launch", "--network", "eth0"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(), HasSubstr("The networks feature is not implemented on this backend"));
}

TEST_P(RefuseBridging, old_image)
{
    const auto [remote, image] = GetParam();
    std::string full_image_name = remote.empty() ? image : remote + ":" + image;

    auto mock_factory = use_a_mock_vm_factory();

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, networks());

    std::stringstream err_stream;
    send_command({"launch", full_image_name, "--network", "eth0"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(), HasSubstr("Automatic network configuration not available for"));
}

std::vector<std::string> old_releases{"10.04",   "lucid",  "11.10",   "oneiric", "12.04", "precise", "12.10",
                                      "quantal", "13.04",  "raring",  "13.10",   "saucy", "14.04",   "trusty",
                                      "14.10",   "utopic", "15.04",   "vivid",   "15.10", "wily",    "16.04",
                                      "xenial",  "16.10",  "yakkety", "17.04",   "zesty"};

std::vector<std::string> old_remoteless_rels{"core", "core16"};

INSTANTIATE_TEST_SUITE_P(DaemonRefuseRelease, RefuseBridging,
                         Combine(Values("release", "daily", ""), ValuesIn(old_releases)));
INSTANTIATE_TEST_SUITE_P(DaemonRefuseRemoteless, RefuseBridging, Combine(Values(""), ValuesIn(old_remoteless_rels)));

TEST_F(Daemon, fails_with_image_not_found_also_if_image_is_also_non_bridgeable)
{
    auto mock_image_host = std::make_unique<NiceMock<mpt::MockImageHost>>();
    auto mock_image_host_ptr = mock_image_host.get();

    std::vector<mp::VMImageHost*> hosts;
    hosts.push_back(mock_image_host_ptr);

    mp::URLDownloader downloader(std::chrono::milliseconds(1));
    mp::DefaultVMImageVault default_vault(hosts, &downloader, mp::Path("/"), mp::Path("/"), mp::days(1));

    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_image_vault_ptr = mock_image_vault.get();

    EXPECT_CALL(*mock_image_vault_ptr, all_info_for(_)).WillOnce([&default_vault](const mp::Query& query) {
        return default_vault.all_info_for(query);
    });

    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();
    EXPECT_CALL(*mock_blueprint_provider, info_for(_)).WillOnce(Return(std::nullopt));

    config_builder.vault = std::move(mock_image_vault);
    config_builder.blueprint_provider = std::move(mock_blueprint_provider);

    mp::Daemon daemon{config_builder.build()};

    std::stringstream err_stream;
    send_command({"launch", "release:xenial", "--network", "eth0"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(), HasSubstr("Unable to find an image matching \"xenial\""));
}

constexpr auto ghost_template = R"(
"{}": {{
    "deleted": false,
    "disk_space": "0",
    "mac_addr": "",
    "mem_size": "0",
    "metadata": {{}},
    "mounts": [],
    "num_cores": 0,
    "ssh_username": "",
    "state": 0
}})";
constexpr auto valid_template = R"(
"{}": {{
    "deleted": false,
    "disk_space": "3232323232",
    "mac_addr": "ab:cd:ef:12:34:{}",
    "mem_size": "2323232323",
    "metadata": {{}},
    "mounts": [],
    "num_cores": 4,
    "ssh_username": "ubuntu",
    "state": 1
}})";
constexpr auto deleted_template = R"(
"{}": {{
    "deleted": true,
    "disk_space": "3232323232",
    "mac_addr": "ab:cd:ef:12:34:{}",
    "mem_size": "2323232323",
    "metadata": {{}},
    "mounts": [],
    "num_cores": 4,
    "ssh_username": "ubuntu",
    "state": 1
}})";

TEST_F(Daemon, skips_over_instance_ghosts_in_db) // which will have been sometimes writen for purged instances
{
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    const auto id1 = "valid1";
    const auto id2 = "valid2";
    auto ghost1 = fmt::format(ghost_template, "ghost1");
    auto ghost2 = fmt::format(ghost_template, "ghost2");
    auto valid1 = fmt::format(valid_template, id1, "56");
    auto valid2 = fmt::format(valid_template, id2, "78");
    const auto [temp_dir, filename] = plant_instance_json(fmt::format(
        "{{\n{},\n{},\n{},\n{}\n}}", std::move(ghost1), std::move(ghost2), std::move(valid1), std::move(valid2)));

    config_builder.data_directory = temp_dir->path();
    auto mock_factory = use_a_mock_vm_factory();

    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(0);
    EXPECT_CALL(*mock_factory, create_virtual_machine(Field(&mp::VirtualMachineDescription::vm_name, id1), _, _))
        .Times(1);
    EXPECT_CALL(*mock_factory, create_virtual_machine(Field(&mp::VirtualMachineDescription::vm_name, id2), _, _))
        .Times(1);

    mp::Daemon daemon{config_builder.build()};
}

TEST_F(Daemon, ctor_lets_exceptions_arising_from_vm_creation_through)
{
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents("ab:ab:ab:ab:ab:ab", {}));
    config_builder.data_directory = temp_dir->path();

    const std::string msg = "asdf";
    auto mock_factory = use_a_mock_vm_factory();
    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Throw(std::runtime_error{msg}));

    MP_EXPECT_THROW_THAT(mp::Daemon{config_builder.build()}, std::runtime_error, mpt::match_what(msg));
}

TEST_F(Daemon, ctor_drops_removed_instances)
{
    const std::string stayed{"foo"}, gone{"fighters"};
    auto stayed_json = fmt::format(valid_template, stayed, "12");
    auto gone_json = fmt::format(valid_template, gone, "34");
    const auto [temp_dir, filename] =
        plant_instance_json(fmt::format("{{\n{},\n{}\n}}", std::move(stayed_json), std::move(gone_json)));
    config_builder.data_directory = temp_dir->path();

    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    EXPECT_CALL(*mock_image_vault, fetch_image(_, Field(&mp::Query::name, stayed), _, _, _, _, _))
        .WillRepeatedly(DoDefault()); // returns an image that can be verified to exist for this instance
    EXPECT_CALL(*mock_image_vault, fetch_image(_, Field(&mp::Query::name, gone), _, _, _, _, _))
        .WillOnce(Return(mp::VMImage{"/path/to/nowhere", "", "", "", "", {}})); // an image that can't be verified to
                                                                                // exist for this instance
    config_builder.vault = std::move(mock_image_vault);

    auto mock_factory = use_a_mock_vm_factory();
    EXPECT_CALL(*mock_factory, create_virtual_machine(Field(&mp::VirtualMachineDescription::vm_name, stayed), _, _))
        .Times(1)
        .WillRepeatedly(
            WithArg<0>([](const auto& desc) { return std::make_unique<mpt::StubVirtualMachine>(desc.vm_name); }));
    EXPECT_CALL(*mock_factory, create_virtual_machine(Field(&mp::VirtualMachineDescription::vm_name, gone), _, _))
        .Times(0);

    mp::Daemon daemon{config_builder.build()};

    StrictMock<mpt::MockServerReaderWriter<mp::ListReply, mp::ListRequest>> mock_server;
    mp::ListReply list_reply;

    auto stayed_matcher = UnorderedElementsAre(Property(&mp::ListVMInstance::name, stayed));
    EXPECT_CALL(mock_server, Write(_, _)).WillOnce(DoAll(SaveArg<0>(&list_reply), Return(true)));

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::list, mp::ListRequest{}, mock_server).ok());
    EXPECT_THAT(list_reply.instance_list().instances(), stayed_matcher);

    auto updated_json = mpt::load(filename);
    EXPECT_THAT(updated_json.toStdString(), AllOf(HasSubstr(stayed), Not(HasSubstr(gone))));
}

TEST_P(ListIP, lists_with_ip)
{
    auto mock_factory = use_a_mock_vm_factory();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>("mock");
    EXPECT_CALL(*mock_factory, create_virtual_machine).WillRepeatedly([&instance_ptr](auto&&...) {
        return std::move(instance_ptr);
    });

    auto [state, cmd, strs] = GetParam();

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(state));
    EXPECT_CALL(*instance_ptr, ensure_vm_is_running()).WillRepeatedly(Throw(std::runtime_error("Not running")));

    MP_DELEGATE_MOCK_CALLS_ON_BASE(mock_utils, is_running, mp::Utils);

    send_command({"launch"});

    std::stringstream stream;
    send_command(cmd, stream);

    for (const auto& s : strs)
        EXPECT_THAT(stream.str(), HasSubstr(s));
}

INSTANTIATE_TEST_SUITE_P(
    Daemon, ListIP,
    Values(std::make_tuple(mp::VirtualMachine::State::running, std::vector<std::string>{"list"},
                           std::vector<std::string>{"Running", "192.168.2.123"}),
           std::make_tuple(mp::VirtualMachine::State::running, std::vector<std::string>{"list", "--no-ipv4"},
                           std::vector<std::string>{"Running", "--"}),
           std::make_tuple(mp::VirtualMachine::State::off, std::vector<std::string>{"list"},
                           std::vector<std::string>{"Stopped", "--"}),
           std::make_tuple(mp::VirtualMachine::State::off, std::vector<std::string>{"list", "--no-ipv4"},
                           std::vector<std::string>{"Stopped", "--"})));

TEST_F(Daemon, prevents_repetition_of_loaded_mac_addresses)
{
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    std::string repeated_mac{"52:54:00:bd:19:41"};
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(repeated_mac, {}));
    config_builder.data_directory = temp_dir->path();

    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(0); // expect *no* call
    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", repeated_mac)}, trash_stream, stream);
    EXPECT_THAT(stream.str(), AllOf(HasSubstr("fail"), HasSubstr("Repeated MAC"), HasSubstr(repeated_mac)));
}

TEST_F(Daemon, does_not_hold_on_to_repeated_mac_addresses_when_loading)
{
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    std::string mac_addr("52:54:00:73:76:28");
    std::vector<mp::NetworkInterface> extra_interfaces{mp::NetworkInterface{"eth0", mac_addr, true}};

    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine);
    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", mac_addr)});
}

TEST_F(Daemon, does_not_hold_on_to_macs_when_loading_fails)
{
    std::string mac1{"52:54:00:73:76:28"}, mac2{"52:54:00:bd:19:41"};
    std::vector<mp::NetworkInterface> extra_interfaces{mp::NetworkInterface{"eth0", mac2, true}};

    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac1, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    EXPECT_CALL(*mock_image_vault, fetch_image)
        .WillOnce(Return(mp::VMImage{"/path/to/nowhere", "", "", "", "", {}})) // cause the Daemon's ctor to fail
                                                                               // verifying that the img exists
        .WillRepeatedly(DoDefault());
    config_builder.vault = std::move(mock_image_vault);

    auto mock_factory = use_a_mock_vm_factory();
    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(2); // no launch in ctor, two launch commands

    mp::Daemon daemon{config_builder.build()};

    for (const auto* mac : {&mac1, &mac2})
        send_command({"launch", "--network", fmt::format("name=eth0,mac={}", *mac)});
}

TEST_F(Daemon, does_not_hold_on_to_macs_when_image_preparation_fails)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    // fail the first prepare call, succeed the second one
    InSequence seq;
    EXPECT_CALL(*mock_factory, prepare_instance_image).WillOnce(Throw(std::exception{})).WillOnce(DoDefault());
    EXPECT_CALL(*mock_factory, create_virtual_machine).Times(1);

    auto cmd = std::vector<std::string>{"launch", "--network", "mac=52:54:00:73:76:28,name=wlan0"};
    send_command(cmd); // we cause this one to fail
    send_command(cmd); // and confirm we can repeat the same mac
}

TEST_F(Daemon, releases_macs_when_launch_fails)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Throw(std::exception{})).WillOnce(DoDefault());

    auto cmd = std::vector<std::string>{"launch", "--network", "mac=52:54:00:73:76:28,name=wlan0"};
    send_command(cmd); // we cause this one to fail
    send_command(cmd); // and confirm we can repeat the same mac
}

TEST_F(Daemon, releases_macs_of_purged_instances_but_keeps_the_rest)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    auto mac1 = "52:54:00:73:76:28", mac2 = "52:54:00:bd:19:41", mac3 = "01:23:45:67:89:ab";

    auto mac_matcher = [](const auto& mac) {
        return Field(&mp::VirtualMachineDescription::extra_interfaces,
                     Contains(Field(&mp::NetworkInterface::mac_address, mac)));
    };
    EXPECT_CALL(*mock_factory, create_virtual_machine(mac_matcher(mac1), _, _)).Times(1);
    EXPECT_CALL(*mock_factory, create_virtual_machine(mac_matcher(mac2), _, _)).Times(1);
    EXPECT_CALL(*mock_factory, create_virtual_machine(mac_matcher(mac3), _, _)).Times(2); // this one gets reused

    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", mac1), "--name", "vm1"});
    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", mac2), "--name", "vm2"});
    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", mac3), "--name", "vm3"});

    send_command({"delete", "vm1"});
    send_command({"delete", "--purge", "vm3"}); // so that mac3 can be reused

    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", mac1)}); // repeated mac is rejected
    send_command({"launch", "--network", fmt::format("name=eth0,mac={}", mac2)}); // idem
    send_command(
        {"launch", "--network", fmt::format("name=eth0,mac={}", mac3)}); // mac is free after purge, so accepted
}

TEST_P(DaemonLaunchTimeoutValueTestSuite, uses_correct_launch_timeout)
{
    auto mock_factory = use_a_mock_vm_factory();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();
    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>("mock");
    EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce([&instance_ptr](auto&&...) {
        return std::move(instance_ptr);
    });

    // Timeouts are given in seconds
    const auto [client_timeout, blueprint_timeout, expected_timeout] = GetParam();

    EXPECT_CALL(*mock_blueprint_provider, blueprint_timeout(_)).WillOnce(Return(blueprint_timeout));

    EXPECT_CALL(*instance_ptr,
                wait_until_ssh_up(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(expected_timeout))))
        .WillRepeatedly(Return());
    EXPECT_CALL(*instance_ptr,
                wait_for_cloud_init(
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(expected_timeout))))
        .WillRepeatedly(Return());

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);

    mp::Daemon daemon{config_builder.build()};

    std::vector<std::string> command{"launch"};
    if (client_timeout > 0)
    {
        command.push_back("--timeout");
        command.push_back(std::to_string(client_timeout));
    }

    send_command(command);
}

INSTANTIATE_TEST_SUITE_P(Daemon, DaemonLaunchTimeoutValueTestSuite,
                         Values(std::make_tuple(0, 600, 600), // client_timeout, blueprint_timeout, expected_timeout
                                std::make_tuple(1000, 600, 1000), std::make_tuple(1000, 0, 1000),
                                std::make_tuple(0, 0, 300)));

TEST_F(Daemon, launches_with_bridged)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, networks());
    EXPECT_CALL(mock_settings, get(Eq(mp::bridged_interface_key))).WillRepeatedly(Return("eth0"));

    ASSERT_NO_THROW(send_command({"launch", "--network", "bridged"}));
}

TEST_F(Daemon, refuses_launch_bridged_without_setting)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(mock_settings, get(Eq(mp::bridged_interface_key))).WillOnce(Return(""));
    EXPECT_CALL(*mock_factory, networks()).Times(0);

    std::stringstream err_stream;
    send_command({"launch", "--network", "bridged"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(),
                HasSubstr("You have to `multipass set local.bridged-network=<name>` to use the \"bridged\" shortcut."));
}

TEST_F(Daemon, refuses_launch_with_invalid_bridged_interface)
{
    mpt::MockVirtualMachineFactory* mock_factory = use_a_mock_vm_factory();

    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, networks());
    EXPECT_CALL(mock_settings, get(Eq(mp::bridged_interface_key))).WillRepeatedly(Return("invalid"));

    std::stringstream err_stream;
    send_command({"launch", "--network", "bridged"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(),
                HasSubstr("Invalid network 'invalid' set as bridged interface, use `multipass set "
                          "local.bridged-network=<name>` to correct. See `multipass networks` for valid names."));
}

TEST_F(Daemon, keysReturnsSettingsKeys)
{
    mp::Daemon daemon{config_builder.build()};

    const auto keys = std::set{"some", "config.keys", "of.various.kinds"};
    EXPECT_CALL(mock_settings, keys).WillOnce(Return(std::set<QString>{keys.begin(), keys.end()}));

    StrictMock<mpt::MockServerReaderWriter<mp::KeysReply, mp::KeysRequest>> mock_server;
    EXPECT_CALL(mock_server, Write(Property(&mp::KeysReply::settings_keys, UnorderedElementsAreArray(keys)), _))
        .WillOnce(Return(true));

    auto status = call_daemon_slot(daemon, &mp::Daemon::keys, mp::KeysRequest{}, mock_server);
    EXPECT_TRUE(status.ok());
}

TEST_F(Daemon, keysReportsException)
{
    mp::Daemon daemon{config_builder.build()};

    const auto e = std::runtime_error{"some error"};
    EXPECT_CALL(mock_settings, keys).WillOnce(Throw(e));

    auto status = call_daemon_slot(daemon, &mp::Daemon::keys, mp::KeysRequest{},
                                   StrictMock<mpt::MockServerReaderWriter<mp::KeysReply, mp::KeysRequest>>{});
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), HasSubstr(e.what()));
}

TEST_F(Daemon, getReturnsSetting)
{
    mp::Daemon daemon{config_builder.build()};

    const auto key = "foo";
    const auto val = "bar";
    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Return(val));

    StrictMock<mpt::MockServerReaderWriter<mp::GetReply, mp::GetRequest>> mock_server;
    EXPECT_CALL(mock_server, Write(Property(&mp::GetReply::value, Eq(val)), _)).WillOnce(Return(true));

    mp::GetRequest request;
    request.set_key(key);

    auto status = call_daemon_slot(daemon, &mp::Daemon::get, request, mock_server);
    EXPECT_TRUE(status.ok());
}

TEST_F(Daemon, getHandlesEmptyKey)
{
    mp::Daemon daemon{config_builder.build()};

    const auto key = "";
    mp::GetRequest request;
    request.set_key(key);

    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
    auto status = call_daemon_slot(daemon, &mp::Daemon::get, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::GetReply, mp::GetRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), AllOf(HasSubstr("Unrecognized"), HasSubstr("''")));
}

TEST_F(Daemon, getHandlesInvalidKey)
{
    mp::Daemon daemon{config_builder.build()};

    const auto key = "bad";
    mp::GetRequest request;
    request.set_key(key);

    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Throw(mp::UnrecognizedSettingException{key}));
    auto status = call_daemon_slot(daemon, &mp::Daemon::get, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::GetReply, mp::GetRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), AllOf(HasSubstr("Unrecognized"), HasSubstr(request.key())));
}

TEST_F(Daemon, getReportsException)
{
    mp::Daemon daemon{config_builder.build()};

    const auto key = "foo";
    mp::GetRequest request;
    request.set_key(key);

    EXPECT_CALL(mock_settings, get(Eq(key))).WillOnce(Throw(std::runtime_error{"exception"}));

    auto status = call_daemon_slot(daemon, &mp::Daemon::get, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::GetReply, mp::GetRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), HasSubstr("exception"));
}

TEST_F(Daemon, setSetsSetting)
{
    mp::Daemon daemon{config_builder.build()};

    const auto key = "foo";
    const auto val = "bar";

    mp::SetRequest request;
    request.set_key(key);
    request.set_val(val);

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(val))).Times(1);

    auto mock_server = StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>{};
    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, mock_server).ok());
}

using SetException = std::variant<mp::UnrecognizedSettingException, mp::InvalidSettingException, std::runtime_error>; /*
  We need to throw an exception of the ultimate type whose handling we're trying to test (to avoid slicing and enter the
  right catch block). Therefore, we can't just use a base exception type. Parameterized and typed tests don't mix in
  gtest, so we use a variant parameter. */
using SetExceptCodeAndMsgMatcher = std::tuple<SetException, grpc::StatusCode, Matcher<std::string>>;
class DaemonSetExceptions : public Daemon, public WithParamInterface<SetExceptCodeAndMsgMatcher>
{
};

TEST_P(DaemonSetExceptions, setHandlesSettingsException)
{
    mp::Daemon daemon{config_builder.build()};

    const auto& [exception, expected_code, msg_matcher] = GetParam();
    auto thrower = [](const auto& e) { throw e; };
    EXPECT_CALL(mock_settings, set).WillOnce(WithoutArgs([&thrower, e = &exception] { std::visit(thrower, *e); })); /*
                                lambda capture with initializer works around forbidden capture of structured binding */

    auto status = call_daemon_slot(daemon, &mp::Daemon::set, mp::SetRequest{},
                                   StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>{});

    EXPECT_EQ(status.error_code(), expected_code);
    EXPECT_THAT(status.error_message(), msg_matcher);
}

INSTANTIATE_TEST_SUITE_P(
    Daemon,
    DaemonSetExceptions,
    Values(std::tuple{mp::NonAuthorizedBridgeSettingsException{"reason", "instance", "eth8"},
                      grpc::StatusCode::INTERNAL,
                      HasSubstr("Need user authorization to bridge eth8")},
           std::tuple{mp::BridgeFailureException{"reason", "instance", "eth8"},
                      grpc::StatusCode::INTERNAL,
                      HasSubstr("Failure to bridge eth8")},
           std::tuple{mp::UnrecognizedSettingException{"foo"},
                      grpc::StatusCode::INVALID_ARGUMENT,
                      AllOf(HasSubstr("Unrecognized"), HasSubstr("foo"))},
           std::tuple{mp::InvalidSettingException{"foo", "bar", "err"},
                      grpc::StatusCode::INVALID_ARGUMENT,
                      AllOf(HasSubstr("Invalid"), HasSubstr("foo"), HasSubstr("bar"), HasSubstr("err"))},
           std::tuple{std::runtime_error{"Other"}, grpc::StatusCode::INTERNAL, HasSubstr("Other")}));

TEST_F(Daemon, setWorksIfUserAuthorizes)
{
    const std::string key{"local.instance.bridged"}, value{"true"};

    mp::Daemon daemon{config_builder.build()};

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                         fmt::format("Asking for user authorization to set {}={}", key, value));
    logger_scope.mock_logger->expect_log(mpl::Level::debug, fmt::format("Succeeded setting {}={}", key, value));

    mp::SetRequest request;
    request.set_key(key);
    request.set_val(value);

    const auto& exception = mp::NonAuthorizedBridgeSettingsException{"reason", "instance", "eth8"};

    EXPECT_CALL(mock_settings, set).WillOnce(Throw(exception)).WillOnce(Return());

    auto mock_server = StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>{};

    EXPECT_CALL(mock_server, Write(_, _)).WillOnce([](mp::SetReply reply, auto) {
        EXPECT_TRUE(reply.needs_authorization());
        return true;
    });

    EXPECT_CALL(mock_server, Read(_)).WillOnce([](mp::SetRequest* request) {
        request->set_authorized(true);
        return true;
    });

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, mock_server).ok());
}

TEST_F(Daemon, set_works_if_bridged_interface_is_empty)
{
    const auto key = "local.instance.cpus";
    const auto value = "8";

    mp::Daemon daemon{config_builder.build()};

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::debug, fmt::format("Succeeded setting {}={}", key, value));

    mp::SetRequest request;
    request.set_key(key);
    request.set_val(value);
    request.set_authorized(true);

    EXPECT_CALL(mock_settings, get(Eq(mp::bridged_interface_key))).WillOnce(Return(""));

    EXPECT_CALL(mock_settings, set(Eq(key), Eq(value))).Times(1);

    auto mock_server = StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>{};

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, mock_server).ok());
}

TEST_F(Daemon, setDoesNotSetIfUserDeniesAuthorization)
{
    const std::string key{"local.instance.bridged"}, value{"true"};

    mp::Daemon daemon{config_builder.build()};

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::debug,
                                         fmt::format("Asking for user authorization to set {}={}", key, value));
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "User did not authorize, cancelling");

    mp::SetRequest request;
    request.set_key(key);
    request.set_val(value);

    const auto& exception = mp::NonAuthorizedBridgeSettingsException{"reason", "instance", "eth8"};

    EXPECT_CALL(mock_settings, set).WillOnce(Throw(exception));

    auto mock_server = StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>{};

    EXPECT_CALL(mock_server, Write(_, _)).WillOnce([](mp::SetReply reply, auto) {
        EXPECT_TRUE(reply.needs_authorization());
        return true;
    });

    EXPECT_CALL(mock_server, Read(_)).WillOnce([](mp::SetRequest* request) {
        request->set_authorized(false);
        return true;
    });

    EXPECT_FALSE(call_daemon_slot(daemon, &mp::Daemon::set, request, mock_server).ok());
}

TEST_F(Daemon, add_bridged_interface_works)
{
    std::string instance_name{"willy"};

    auto mock_factory = use_a_mock_vm_factory();
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "New interface {\"eth8\", ");

    std::vector<mp::NetworkInterfaceInfo> net_info{{"eth8", "Ethernet", "A network adapter", {}, false}};
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(net_info));
    EXPECT_CALL(*mock_factory, prepare_networking).Times(1);
    EXPECT_CALL(*instance_ptr, add_network_interface(0, _, _)).Times(1);

    EXPECT_NO_THROW(daemon.test_add_bridged_interface(instance_name, instance_ptr));
}

TEST_F(Daemon, add_bridged_interface_warns_and_noop_if_already_bridged)
{
    std::string instance_name{"foo"};
    std::string if_name{"eth8"};

    mp::VMSpecs specs{};
    specs.extra_interfaces.push_back({if_name, "ab:ab:ab:ab:ab:ab", true});

    auto mock_factory = use_a_mock_vm_factory();
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "already bridged");

    EXPECT_CALL(*mock_factory, networks).Times(1);
    EXPECT_CALL(*mock_factory, prepare_networking).Times(0);
    EXPECT_CALL(*instance_ptr, add_network_interface).Times(0);

    EXPECT_NO_THROW(daemon.test_add_bridged_interface(instance_name, instance_ptr, specs));
}

TEST_F(Daemon, add_bridged_interface_honors_prepared_bridge)
{
    std::string instance_name{"asdf"};
    std::string if_name{"eth8"};
    std::string br_name{"br-eth8"};
    mp::NetworkInterface br_net{br_name, "ab:ab:ab:ab:ab:ab", true};

    auto mock_factory = use_a_mock_vm_factory();
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    std::vector<mp::NetworkInterfaceInfo> net_info{{if_name, "Ethernet", "A regular adapter", {}, false}};
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(net_info));
    EXPECT_CALL(*mock_factory, prepare_networking(Contains(Field(&mp::NetworkInterface::id, StrEq("eth8")))))
        .WillOnce(SetArgReferee<0>(std::vector{br_net}));
    EXPECT_CALL(*instance_ptr, add_network_interface(0, _, Eq(br_net))).Times(1);

    EXPECT_NO_THROW(daemon.test_add_bridged_interface(instance_name, instance_ptr));
}

TEST_F(Daemon, add_bridged_interface_throws_if_backend_throws)
{
    std::string instance_name{"wonka"};

    auto mock_factory = use_a_mock_vm_factory();
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::debug);
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "New interface {\"eth8\", ");
    logger_scope.mock_logger->expect_log(mpl::Level::debug, "Failure adding interface to instance, rolling back");
    std::vector<mp::NetworkInterfaceInfo> net_info{{"eth8", "Ethernet", "A network adapter", {}, false}};
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(net_info));
    EXPECT_CALL(*mock_factory, prepare_networking).Times(1);
    EXPECT_CALL(*instance_ptr, add_network_interface(0, _, _)).WillOnce(Throw(std::runtime_error("something bad")));

    std::string msg{"Cannot update instance settings; instance: " + instance_name + "; reason: Failure to bridge eth8"};
    MP_EXPECT_THROW_THAT(daemon.test_add_bridged_interface(instance_name, instance_ptr),
                         std::runtime_error,
                         mpt::match_what(msg));
}

TEST_F(Daemon, add_bridged_interface_throws_on_bad_bridged_network_setting)
{
    std::string instance_name{"bucket"};

    auto mock_factory = use_a_mock_vm_factory();
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    std::vector<mp::NetworkInterfaceInfo> net_info{{"eth9", "Ethernet", "An invalid network adapter", {}, false}};
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(net_info));
    EXPECT_CALL(*mock_factory, prepare_networking).Times(0);
    EXPECT_CALL(*instance_ptr, add_network_interface(_, _, _)).Times(0);

    std::string msg{"Invalid network 'eth8' set as bridged interface, use `multipass set local.bridged-network=<name>` "
                    "to correct. See `multipass networks` for valid names."};
    MP_EXPECT_THROW_THAT(daemon.test_add_bridged_interface(instance_name, instance_ptr),
                         std::runtime_error,
                         mpt::match_what(msg));
}

TEST_F(Daemon, add_bridged_interface_throws_if_needs_authorization)
{
    std::string instance_name{"glass-elevator"};

    auto mock_factory = use_a_mock_vm_factory();
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    std::vector<mp::NetworkInterfaceInfo> net_info{{"eth8", "Ethernet", "A network adapter", {}, true}};
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(net_info));
    EXPECT_CALL(*mock_factory, prepare_networking).Times(0);
    EXPECT_CALL(*instance_ptr, add_network_interface(_, _, _)).Times(0);

    std::string msg{
        "Cannot update instance settings; instance: glass-elevator; reason: Need user authorization to bridge eth8"};
    MP_EXPECT_THROW_THAT(daemon.test_add_bridged_interface(instance_name, instance_ptr),
                         mp::NonAuthorizedBridgeSettingsException,
                         mpt::match_what(msg));
}

struct DaemonIsBridged : public Daemon,
                         public WithParamInterface<
                             std::tuple<std::vector<mp::NetworkInterfaceInfo>, std::vector<mp::NetworkInterface>, bool>>
{
};

TEST_P(DaemonIsBridged, is_bridged_works)
{
    const auto [host_nets, extra_interfaces, result] = GetParam();

    std::string instance_name{"charlie"};

    mp::VMSpecs specs{};
    specs.extra_interfaces = extra_interfaces;

    auto [mock_platform, platform_guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, bridge_nomenclature).WillRepeatedly(Return("generic"));

    auto mock_factory = use_a_mock_vm_factory();
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(host_nets));
    mpt::MockDaemon daemon{config_builder.build()};
    auto instance_ptr = std::make_shared<NiceMock<mpt::MockVirtualMachine>>(instance_name);

    EXPECT_EQ(daemon.test_is_bridged(instance_name, specs), result);
}

INSTANTIATE_TEST_SUITE_P(
    Daemon,
    DaemonIsBridged,
    Values(std::make_tuple(std::vector<mp::NetworkInterfaceInfo>{},
                           std::vector<mp::NetworkInterface>{{"eth8", "52:54:00:09:10:11", true}},
                           true),
           std::make_tuple(std::vector<mp::NetworkInterfaceInfo>{{"somebr", "generic", "some bridge", {"eth8"}, false}},
                           std::vector<mp::NetworkInterface>{{"somebr", "52:54:00:12:13:14", true}},
                           true),
           std::make_tuple(std::vector<mp::NetworkInterfaceInfo>{},
                           std::vector<mp::NetworkInterface>{{"eth9", "52:54:00:15:16:17", true}},
                           false),
           std::make_tuple(std::vector<mp::NetworkInterfaceInfo>{{"somebr", "generic", "some bridge", {"eth9"}, false}},
                           std::vector<mp::NetworkInterface>{{"somebr", "52:54:00:18:19:20", true}},
                           false)));

TEST_F(Daemon, requests_networks)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    std::vector<mp::NetworkInterfaceInfo> net_infos{{"net_a", "type_a", "description_a"},
                                                    {"net_b", "type_b", "description_b"}};
    EXPECT_CALL(*mock_factory, networks).WillOnce(Return(net_infos));

    StrictMock<mpt::MockServerReaderWriter<mp::NetworksReply, mp::NetworksRequest>> mock_server;

    auto are_same_net = [](const mp::NetInterface& proto_net, const mp::NetworkInterfaceInfo& net_info) {
        return std::tie(proto_net.name(), proto_net.type(), proto_net.description()) ==
               std::tie(net_info.id, net_info.type, net_info.description);
    };
    auto same_net_matcher = Truly(mpt::with_arg_tuple(are_same_net)); // matches pairs (2-tuples) of equivalent nets

    EXPECT_CALL(mock_server,
                Write(Property(&mp::NetworksReply::interfaces, UnorderedPointwise(same_net_matcher, net_infos)), _))
        .WillOnce(Return(true));

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::networks, mp::NetworksRequest{}, mock_server).ok());
}

TEST_F(Daemon, performs_health_check_on_networks)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, hypervisor_health_check);
    call_daemon_slot(daemon, &mp::Daemon::networks, mp::NetworksRequest{},
                     NiceMock<mpt::MockServerReaderWriter<mp::NetworksReply, mp::NetworksRequest>>{});
}

TEST_F(Daemon, purgePersistsInstances)
{
    const std::string name1{"world-of-goo"}, name2{"small-beauty-goo"};
    auto instance_json1 = fmt::format(valid_template, name1, "10");
    auto instance_json2 = fmt::format(valid_template, name2, "11");
    auto json_contents = fmt::format("{{{}, {}}}", instance_json1, instance_json2);

    const auto [temp_dir, filename] = plant_instance_json(json_contents);
    config_builder.data_directory = temp_dir->path();

    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    mp::Daemon daemon{config_builder.build()};

    QFile::remove(filename);
    call_daemon_slot(daemon, &mp::Daemon::purge, mp::PurgeRequest{},
                     NiceMock<mpt::MockServerReaderWriter<mp::PurgeReply, mp::PurgeRequest>>{});

    auto updated_json = mpt::load(filename);
    EXPECT_THAT(updated_json.toStdString(), AllOf(HasSubstr(name1), HasSubstr(name2)));
}

TEST_F(Daemon, launch_fails_with_incompatible_blueprint)
{
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();
    EXPECT_CALL(*mock_blueprint_provider, info_for(_)).WillOnce(Throw(mp::IncompatibleBlueprintException("")));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);

    mp::Daemon daemon{config_builder.build()};

    std::stringstream err_stream;
    send_command({"launch", "foo"}, trash_stream, err_stream);
    EXPECT_THAT(err_stream.str(), HasSubstr("The \"foo\" Blueprint is not compatible with this host."));
}

TEST_F(Daemon, info_all_returns_all_instances)
{
    const std::string good_instance_name{"good-instance"}, deleted_instance_name{"deleted-instance"};
    const auto good_instance_json = fmt::format(valid_template, good_instance_name, "10");
    const auto deleted_instance_json = fmt::format(deleted_template, deleted_instance_name, "11");
    const auto instances_json = fmt::format("{{{}, {}}}", good_instance_json, deleted_instance_json);
    const auto [temp_dir, __] = plant_instance_json(instances_json);
    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    EXPECT_CALL(*use_a_mock_vm_factory(), create_virtual_machine).WillRepeatedly(WithArg<0>([](const auto& desc) {
        return std::make_unique<mpt::StubVirtualMachine>(desc.vm_name);
    }));

    const auto names_matcher = UnorderedElementsAre(Property(&mp::DetailedInfoItem::name, good_instance_name),
                                                    Property(&mp::DetailedInfoItem::name, deleted_instance_name));

    StrictMock<mpt::MockServerReaderWriter<mp::InfoReply, mp::InfoRequest>> mock_server{};
    EXPECT_CALL(mock_server, Write(Property(&mp::InfoReply::details, names_matcher), _)).WillOnce(Return(true));

    mp::Daemon daemon{config_builder.build()};
    call_daemon_slot(daemon, &mp::Daemon::info, mp::InfoRequest{}, mock_server);
}
} // namespace
