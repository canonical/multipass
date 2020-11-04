/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include <src/client/cli/client.h>
#include <src/daemon/daemon.h>
#include <src/daemon/daemon_config.h>
#include <src/daemon/daemon_rpc.h>
#include <src/platform/update/disabled_update_prompt.h>

#include <multipass/auto_join_thread.h>
#include <multipass/cli/argparser.h>
#include <multipass/cli/command.h>
#include <multipass/constants.h>
#include <multipass/name_generator.h>
#include <multipass/version.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>

#include "extra_assertions.h"
#include "mock_environment_helpers.h"
#include "mock_process_factory.h"
#include "mock_standard_paths.h"
#include "mock_virtual_machine_factory.h"
#include "mock_vm_image_vault.h"
#include "stub_cert_store.h"
#include "stub_certprovider.h"
#include "stub_image_host.h"
#include "stub_logger.h"
#include "stub_ssh_key_provider.h"
#include "stub_terminal.h"
#include "stub_virtual_machine_factory.h"
#include "stub_vm_image_vault.h"
#include "temp_dir.h"

#include <yaml-cpp/yaml.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QNetworkProxyFactory>
#include <QSysInfo>

#include <scope_guard.hpp>

#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

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
template<typename R>
  bool is_ready(std::future<R> const& f)
  { return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }

struct MockDaemon : public mp::Daemon
{
    using mp::Daemon::Daemon;

    MOCK_METHOD3(create,
                 void(const mp::CreateRequest*, grpc::ServerWriter<mp::CreateReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(launch,
                 void(const mp::LaunchRequest*, grpc::ServerWriter<mp::LaunchReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(purge,
                 void(const mp::PurgeRequest*, grpc::ServerWriter<mp::PurgeReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(find,
                 void(const mp::FindRequest* request, grpc::ServerWriter<mp::FindReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(info, void(const mp::InfoRequest*, grpc::ServerWriter<mp::InfoReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(list, void(const mp::ListRequest*, grpc::ServerWriter<mp::ListReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(mount, void(const mp::MountRequest* request, grpc::ServerWriter<mp::MountReply>*,
                             std::promise<grpc::Status>*));
    MOCK_METHOD3(recover,
                 void(const mp::RecoverRequest*, grpc::ServerWriter<mp::RecoverReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(ssh_info,
                 void(const mp::SSHInfoRequest*, grpc::ServerWriter<mp::SSHInfoReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(start,
                 void(const mp::StartRequest*, grpc::ServerWriter<mp::StartReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(stop, void(const mp::StopRequest*, grpc::ServerWriter<mp::StopReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(suspend,
                 void(const mp::SuspendRequest*, grpc::ServerWriter<mp::SuspendReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(restart,
                 void(const mp::RestartRequest*, grpc::ServerWriter<mp::RestartReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(delet,
                 void(const mp::DeleteRequest*, grpc::ServerWriter<mp::DeleteReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(umount,
                 void(const mp::UmountRequest*, grpc::ServerWriter<mp::UmountReply>*, std::promise<grpc::Status>*));
    MOCK_METHOD3(version,
                 void(const mp::VersionRequest*, grpc::ServerWriter<mp::VersionReply>*, std::promise<grpc::Status>*));

    template <typename Request, typename Reply>
    void set_promise_value(const Request*, grpc::ServerWriter<Reply>*, std::promise<grpc::Status>* status_promise)
    {
        status_promise->set_value(grpc::Status::OK);
    }
};

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

class TestCreate final : public mp::cmd::Command
{
public:
    using Command::Command;
    mp::ReturnCode run(mp::ArgParser* parser) override
    {
        auto on_success = [](mp::CreateReply& /*reply*/) { return mp::ReturnCode::Ok; };
        auto on_failure = [this](grpc::Status& status) {
            mp::CreateError create_error;
            create_error.ParseFromString(status.error_details());
            const auto errors = create_error.error_codes();

            cerr << "failed: " << status.error_message();
            if (errors.size() == 1)
            {
                const auto& error = errors[0];
                if (error == mp::CreateError::INVALID_DISK_SIZE)
                    cerr << "disk";
                else if (error == mp::CreateError::INVALID_MEM_SIZE)
                    cerr << "memory";
                else
                    cerr << "?";
            }

            return mp::ReturnCode::CommandFail;
        };

        auto streaming_callback = [this](mp::CreateReply& reply) { cout << reply.create_message() << std::endl; };

        auto ret = parse_args(parser);
        return ret == mp::ParseCode::Ok
                   ? dispatch(&mp::Rpc::Stub::create, request, on_success, on_failure, streaming_callback)
                   : parser->returnCodeFrom(ret);
    }

    std::string name() const override
    {
        return "test_create";
    }

    QString short_help() const override
    {
        return {};
    }

    QString description() const override
    {
        return {};
    }

private:
    mp::ParseCode parse_args(mp::ArgParser* parser) override
    {
        QCommandLineOption diskOption("disk", "", "disk", "");
        QCommandLineOption memOption("mem", "", "mem", "");
        parser->addOptions({diskOption, memOption});

        auto status = parser->commandParse(this);
        if (status == mp::ParseCode::Ok)
        {
            if (parser->isSet(memOption))
                request.set_mem_size(parser->value(memOption).toStdString());

            if (parser->isSet(diskOption))
                request.set_disk_space(parser->value(diskOption).toStdString());
        }

        return status;
    }

    mp::CreateRequest request;
};

class TestClient : public mp::Client
{
public:
    explicit TestClient(mp::ClientConfig& context) : mp::Client{context}
    {
        add_command<TestCreate>();
        sort_commands();
    }
};

} // namespace

struct Daemon : public Test
{
    Daemon()
    {
        config_builder.server_address = server_address;
        config_builder.cache_directory = cache_dir.path();
        config_builder.data_directory = data_dir.path();
        config_builder.vault = std::make_unique<mpt::StubVMImageVault>();
        config_builder.factory = std::make_unique<mpt::StubVirtualMachineFactory>();
        config_builder.image_hosts.push_back(std::make_unique<mpt::StubVMImageHost>());
        config_builder.ssh_key_provider = std::make_unique<mpt::StubSSHKeyProvider>();
        config_builder.cert_provider = std::make_unique<mpt::StubCertProvider>();
        config_builder.client_cert_store = std::make_unique<mpt::StubCertStore>();
        config_builder.connection_type = mp::RpcConnectionType::insecure;
        config_builder.logger = std::make_unique<mpt::StubLogger>();
        config_builder.update_prompt = std::make_unique<mp::DisabledUpdatePrompt>();
    }

    void SetUp() override
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), locate(_, _, _))
            .Times(AnyNumber()); // needed to allow general calls once we have added the specific expectation below
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                    locate(_, Property(&QString::toStdString, EndsWith("settings.json")), _))
            .Times(AnyNumber())
            .WillRepeatedly(Return("")); /* Avoid writing to Windows Terminal settings. We use an "expectation" so that
                                            it gets reset at the end of each test (by VerifyAndClearExpectations) */
    }

    mpt::MockVirtualMachineFactory* use_a_mock_vm_factory()
    {
        auto mock_factory = std::make_unique<NiceMock<mpt::MockVirtualMachineFactory>>();
        auto mock_factory_ptr = mock_factory.get();

        ON_CALL(*mock_factory_ptr, fetch_type()).WillByDefault(Return(mp::FetchType::ImageOnly));

        ON_CALL(*mock_factory_ptr, create_virtual_machine(_, _))
            .WillByDefault(Return(ByMove(std::make_unique<mpt::StubVirtualMachine>())));

        ON_CALL(*mock_factory_ptr, prepare_source_image(_)).WillByDefault(ReturnArg<0>());

        ON_CALL(*mock_factory_ptr, get_backend_version_string()).WillByDefault(Return("mock-1234"));

        config_builder.factory = std::move(mock_factory);
        return mock_factory_ptr;
    }

    void send_command(const std::vector<std::string>& command, std::ostream& cout = trash_stream,
                      std::ostream& cerr = trash_stream, std::istream& cin = trash_stream)
    {
        send_commands({command}, cout, cerr, cin);
    }

    // "commands" is a vector of commands that includes necessary positional arguments, ie,
    // "start foo"
    void send_commands(std::vector<std::vector<std::string>> commands, std::ostream& cout = trash_stream,
                       std::ostream& cerr = trash_stream, std::istream& cin = trash_stream)
    {
        // Commands need to be sent from a thread different from that the QEventLoop is on.
        // Event loop is started/stopped to ensure all signals are delivered
        mp::AutoJoinThread t([this, &commands, &cout, &cerr, &cin] {
            mpt::StubTerminal term(cout, cerr, cin);
            mp::ClientConfig client_config{server_address, mp::RpcConnectionType::insecure,
                                           std::make_unique<mpt::StubCertProvider>(), &term};
            TestClient client{client_config};
            for (const auto& command : commands)
            {
                QStringList args = QStringList() << "multipass_test";

                for (const auto& arg : command)
                {
                    args << QString::fromStdString(arg);
                }
                client.run(args);
            }
            loop.quit();
        });
        loop.exec();
    }

    std::string server_address{"unix:/tmp/test-multipassd.socket"};
    QEventLoop loop; // needed as signal/slots used internally by mp::Daemon
    mpt::TempDir cache_dir;
    mpt::TempDir data_dir;
    mp::DaemonConfigBuilder config_builder;
    inline static std::stringstream trash_stream{}; // this may have contents (that we don't care about)
};

TEST_F(Daemon, receives_commands)
{
    MockDaemon daemon{config_builder.build()};

    EXPECT_CALL(daemon, create(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::CreateRequest, mp::CreateReply>));
    EXPECT_CALL(daemon, launch(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::LaunchRequest, mp::LaunchReply>));
    EXPECT_CALL(daemon, purge(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::PurgeRequest, mp::PurgeReply>));
    EXPECT_CALL(daemon, find(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::FindRequest, mp::FindReply>));
    EXPECT_CALL(daemon, ssh_info(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::SSHInfoRequest, mp::SSHInfoReply>));
    EXPECT_CALL(daemon, info(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::InfoRequest, mp::InfoReply>));
    EXPECT_CALL(daemon, list(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::ListRequest, mp::ListReply>));
    EXPECT_CALL(daemon, recover(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::RecoverRequest, mp::RecoverReply>));
    EXPECT_CALL(daemon, start(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::StartRequest, mp::StartReply>));
    EXPECT_CALL(daemon, stop(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::StopRequest, mp::StopReply>));
    EXPECT_CALL(daemon, suspend(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::SuspendRequest, mp::SuspendReply>));
    EXPECT_CALL(daemon, restart(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::RestartRequest, mp::RestartReply>));
    EXPECT_CALL(daemon, delet(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::DeleteRequest, mp::DeleteReply>));
    EXPECT_CALL(daemon, version(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::VersionRequest, mp::VersionReply>));
    EXPECT_CALL(daemon, mount(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::MountRequest, mp::MountReply>));
    EXPECT_CALL(daemon, umount(_, _, _))
        .WillOnce(Invoke(&daemon, &MockDaemon::set_promise_value<mp::UmountRequest, mp::UmountReply>));

    send_commands({{"test_create", "foo"},
                   {"launch", "foo"},
                   {"delete", "foo"},
                   {"exec", "foo", "--", "cmd"},
                   {"info", "foo"},
                   {"list"},
                   {"purge"},
                   {"recover", "foo"},
                   {"start", "foo"},
                   {"stop", "foo"},
                   {"suspend", "foo"},
                   {"restart", "foo"},
                   {"version"},
                   {"find", "something"},
                   {"mount", ".", "target"},
                   {"umount", "instance"}});
}

TEST_F(Daemon, provides_version)
{
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"version"}, stream);

    EXPECT_THAT(stream.str(), HasSubstr(mp::version_string));
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
    auto guard = sg::make_scope_guard([] {
        // Resets proxy back to what the system is configured for
        QNetworkProxyFactory::setUseSystemConfiguration(true);
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

TEST_F(Daemon, data_path_valid)
{
    QTemporaryDir data_dir, cache_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::CacheLocation))
        .WillOnce(Return(cache_dir.path()));

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::AppDataLocation))
        .WillOnce(Return(data_dir.path()));

    config_builder.data_directory = "";
    config_builder.cache_directory = "";
    auto config = config_builder.build();

    EXPECT_EQ(config->data_directory.toStdString(), data_dir.path().toStdString());
    EXPECT_EQ(config->cache_directory.toStdString(), cache_dir.path().toStdString());
}

TEST_F(Daemon, data_path_with_storage_valid)
{
    QTemporaryDir storage_dir;

    mpt::SetEnvScope storage("MULTIPASS_STORAGE", storage_dir.path().toUtf8());
    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_)).Times(0);

    config_builder.data_directory = "";
    config_builder.cache_directory = "";
    auto config = config_builder.build();

    EXPECT_EQ(config->data_directory.toStdString(), storage_dir.filePath("data").toStdString());
    EXPECT_EQ(config->cache_directory.toStdString(), storage_dir.filePath("cache").toStdString());
}

namespace
{
struct DaemonCreateLaunchTestSuite : public Daemon, public WithParamInterface<std::string>
{
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

struct LaunchWithBridges
    : public Daemon,
      public WithParamInterface<
          std::pair<std::vector<std::tuple<std::string, std::string, std::string>>, std::vector<std::string>>>
{
};

TEST_P(DaemonCreateLaunchTestSuite, creates_virtual_machines)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _));
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

class DummyKeyProvider : public mpt::StubSSHKeyProvider
{
public:
    explicit DummyKeyProvider(std::string key) : key{std::move(key)}
    {
    }
    std::string public_key_as_base64() const override
    {
        return key;
    };

private:
    std::string key;
};

TEST_P(DaemonCreateLaunchTestSuite, adds_ssh_keys_to_cloud_init_config)
{
    auto mock_factory = use_a_mock_vm_factory();
    std::string expected_key{"thisitnotansshkeyactually"};
    config_builder.ssh_key_provider = std::make_unique<DummyKeyProvider>(expected_key);
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _))
        .WillOnce(Invoke([&expected_key](const multipass::VMImage&, const mp::VirtualMachineDescription& desc) {
            ASSERT_THAT(desc.vendor_data_config, YAMLNodeContainsSequence("ssh_authorized_keys"));
            auto const& ssh_keys_stanza = desc.vendor_data_config["ssh_authorized_keys"];
            EXPECT_THAT(ssh_keys_stanza, YAMLNodeContainsSubString(expected_key));
        }));

    send_command({GetParam()});
}

TEST_P(DaemonCreateLaunchTestSuite, adds_pollinate_user_agent_to_cloud_init_config)
{
    auto mock_factory = use_a_mock_vm_factory();
    std::vector<std::pair<std::string, std::string>> const& expected_pollinate_map{
        {"path", "/etc/pollinate/add-user-agent"},
        {"content", fmt::format("multipass/version/{} # written by Multipass\n"
                                "multipass/driver/mock-1234 # written by Multipass\n"
                                "multipass/host/{}-{} # written by Multipass\n",
                                multipass::version_string, QSysInfo::productType(), QSysInfo::productVersion())},
    };
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

    send_command({GetParam()});
}

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
    Values(BridgeTestArgType({}, {"extra0"}), BridgeTestArgType({{"eth0", "extra0", "52:54:00:"}}, {"extra1"}),
           BridgeTestArgType({{"id=eth0,mac=01:23:45:ab:cd:ef,mode=auto", "extra0", "01:23:45:ab:cd:ef"},
                              {"wlan0", "extra1", "52:54:00:"}},
                             {"extra2"}),
           BridgeTestArgType({{"id=eth0,mode=manual", "", ""}}, {"extra0"}),
           BridgeTestArgType({{"id=eth0,mode=manual", "", ""}, {"id=wlan1", "extra1", "52:54:00:"}},
                             {"extra0", "extra2"})));

TEST_P(MinSpaceRespectedSuite, accepts_launch_with_enough_explicit_memory)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    const auto param = GetParam();
    const auto& cmd = std::get<0>(param);
    const auto& opt_name = std::get<1>(param);
    const auto& opt_value = std::get<2>(param);

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _));
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

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).Times(0); // expect *no* call
    send_command({cmd, opt_name, opt_value}, std::cout, stream);
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

    config_builder.vault = std::move(mock_image_vault);
    mp::Daemon daemon{config_builder.build()};

    std::vector<std::string> all_parameters{first_command_line_parameter};
    for (const auto& p : other_command_line_parameters)
        all_parameters.push_back(p);

    if (other_command_line_parameters.size() > 0 && mp::MemorySize(other_command_line_parameters[1]) < img_size)
    {
        std::stringstream stream;
        EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).Times(0);
        send_command(all_parameters, trash_stream, stream);
        EXPECT_THAT(stream.str(), AllOf(HasSubstr("Requested disk"), HasSubstr("below minimum for this image")));
    }
    else
    {
        EXPECT_CALL(*mock_factory, create_virtual_machine(_, _));
        send_command(all_parameters);
    }
}

INSTANTIATE_TEST_SUITE_P(Daemon, DaemonCreateLaunchTestSuite, Values("launch", "test_create"));
INSTANTIATE_TEST_SUITE_P(Daemon, MinSpaceRespectedSuite,
                         Combine(Values("test_create", "launch"), Values("--mem", "--disk"),
                                 Values("1024m", "2Gb", "987654321")));
INSTANTIATE_TEST_SUITE_P(Daemon, MinSpaceViolatedSuite,
                         Combine(Values("test_create", "launch"), Values("--mem", "--disk"),
                                 Values("0", "0B", "0GB", "123B", "42kb", "100")));
INSTANTIATE_TEST_SUITE_P(Daemon, LaunchImgSizeSuite,
                         Combine(Values("test_create", "launch"),
                                 Values(std::vector<std::string>{}, std::vector<std::string>{"--disk", "4G"}),
                                 Values("1G", mp::default_disk_size, "10G")));

} // namespace
