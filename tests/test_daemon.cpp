/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include <src/client/client.h>
#include <src/daemon/daemon.h>
#include <src/daemon/daemon_config.h>

#include <multipass/auto_join_thread.h>
#include <multipass/name_generator.h>
#include <multipass/version.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_host.h>
#include <multipass/vm_image_vault.h>

#include "mock_virtual_machine_factory.h"
#include "stub_cert_store.h"
#include "stub_certprovider.h"
#include "stub_image_host.h"
#include "stub_logger.h"
#include "stub_ssh_key_provider.h"
#include "stub_virtual_machine_factory.h"
#include "stub_vm_image_vault.h"
#include "temp_dir.h"

#include <yaml-cpp/yaml.h>

#include <gtest/gtest.h>

#include <QCoreApplication>

#include <memory>
#include <sstream>
#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct MockDaemon : public mp::Daemon
{
    using mp::Daemon::Daemon;
    MOCK_METHOD3(launch,
                 grpc::Status(grpc::ServerContext*, const mp::LaunchRequest*, grpc::ServerWriter<mp::LaunchReply>*));
    MOCK_METHOD3(purge,
                 grpc::Status(grpc::ServerContext*, const mp::PurgeRequest*, grpc::ServerWriter<mp::PurgeReply>*));
    MOCK_METHOD3(find, grpc::Status(grpc::ServerContext* context, const mp::FindRequest* request,
                                    grpc::ServerWriter<mp::FindReply>*));
    MOCK_METHOD3(info, grpc::Status(grpc::ServerContext*, const mp::InfoRequest*, grpc::ServerWriter<mp::InfoReply>*));
    MOCK_METHOD3(list, grpc::Status(grpc::ServerContext*, const mp::ListRequest*, grpc::ServerWriter<mp::ListReply>*));
    MOCK_METHOD3(mount, grpc::Status(grpc::ServerContext* context, const mp::MountRequest* request,
                                     grpc::ServerWriter<mp::MountReply>*));
    MOCK_METHOD3(recover,
                 grpc::Status(grpc::ServerContext*, const mp::RecoverRequest*, grpc::ServerWriter<mp::RecoverReply>*));
    MOCK_METHOD3(ssh_info,
                 grpc::Status(grpc::ServerContext*, const mp::SSHInfoRequest*, grpc::ServerWriter<mp::SSHInfoReply>*));
    MOCK_METHOD3(start,
                 grpc::Status(grpc::ServerContext*, const mp::StartRequest*, grpc::ServerWriter<mp::StartReply>*));
    MOCK_METHOD3(stop, grpc::Status(grpc::ServerContext*, const mp::StopRequest*, grpc::ServerWriter<mp::StopReply>*));
    MOCK_METHOD3(suspend,
                 grpc::Status(grpc::ServerContext*, const mp::SuspendRequest*, grpc::ServerWriter<mp::SuspendReply>*));
    MOCK_METHOD3(restart,
                 grpc::Status(grpc::ServerContext*, const mp::RestartRequest*, grpc::ServerWriter<mp::RestartReply>*));
    MOCK_METHOD3(delet,
                 grpc::Status(grpc::ServerContext*, const mp::DeleteRequest*, grpc::ServerWriter<mp::DeleteReply>*));
    MOCK_METHOD3(umount, grpc::Status(grpc::ServerContext* context, const mp::UmountRequest* request,
                                      grpc::ServerWriter<mp::UmountReply>* response));
    MOCK_METHOD3(version,
                 grpc::Status(grpc::ServerContext*, const mp::VersionRequest*, grpc::ServerWriter<mp::VersionReply>*));
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
    }

    mpt::MockVirtualMachineFactory* use_a_mock_vm_factory()
    {
        auto mock_factory = std::make_unique<NiceMock<mpt::MockVirtualMachineFactory>>();
        auto mock_factory_ptr = mock_factory.get();

        ON_CALL(*mock_factory_ptr, create_virtual_machine(_, _))
            .WillByDefault(Return(ByMove(std::make_unique<mpt::StubVirtualMachine>())));

        ON_CALL(*mock_factory_ptr, prepare_source_image(_)).WillByDefault(ReturnArg<0>());

        config_builder.factory = std::move(mock_factory);
        return mock_factory_ptr;
    }

    void send_command(const std::vector<std::string>& command)
    {
        send_command(command, null_stream);
    }

    void send_command(const std::vector<std::string>& command, std::ostream& cout)
    {
        send_commands({command}, cout);
    }

    void send_commands(const std::vector<std::vector<std::string>>& commands)
    {
        send_commands(commands, null_stream);
    }

    // "commands" is a vector of commands that includes necessary positional arguments, ie,
    // "start foo"
    void send_commands(std::vector<std::vector<std::string>> commands, std::ostream& cout)
    {
        // Commands need to be sent from a thread different from that the QEventLoop is on.
        // Event loop is started/stopped to ensure all signals are delivered
        mp::AutoJoinThread t([this, &commands, &cout] {
            mp::ClientConfig client_config{server_address, mp::RpcConnectionType::insecure,
                                           std::make_unique<mpt::StubCertProvider>(), cout, std::cerr};
            mp::Client client{client_config};
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
    std::stringstream null_stream;
};

TEST_F(Daemon, receives_commands)
{
    MockDaemon daemon{config_builder.build()};

    EXPECT_CALL(daemon, launch(_, _, _));
    EXPECT_CALL(daemon, purge(_, _, _));
    EXPECT_CALL(daemon, find(_, _, _));
    EXPECT_CALL(daemon, ssh_info(_, _, _));
    EXPECT_CALL(daemon, info(_, _, _));
    EXPECT_CALL(daemon, list(_, _, _));
    EXPECT_CALL(daemon, recover(_, _, _));
    EXPECT_CALL(daemon, start(_, _, _));
    EXPECT_CALL(daemon, stop(_, _, _));
    EXPECT_CALL(daemon, suspend(_, _, _));
    EXPECT_CALL(daemon, restart(_, _, _));
    EXPECT_CALL(daemon, delet(_, _, _));
    EXPECT_CALL(daemon, version(_, _, _));
    EXPECT_CALL(daemon, mount(_, _, _));
    EXPECT_CALL(daemon, umount(_, _, _));

    send_commands({{"launch", "foo"},
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

TEST_F(Daemon, creates_virtual_machines)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _));
    send_command({"launch"});
}

TEST_F(Daemon, on_creation_hooks_up_platform_prepare_source_image)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_source_image(_));
    send_command({"launch"});
}

TEST_F(Daemon, on_creation_hooks_up_platform_prepare_instance_image)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, prepare_instance_image(_, _));
    send_command({"launch"});
}

TEST_F(Daemon, provides_version)
{
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"version"}, stream);

    EXPECT_THAT(stream.str(), HasSubstr(mp::version_string));
}

TEST_F(Daemon, generates_name_when_client_does_not_provide_one)
{
    const std::string expected_name{"pied-piper-valley"};

    config_builder.name_generator = std::make_unique<StubNameGenerator>(expected_name);
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;
    send_command({"launch"}, stream);

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

TEST_F(Daemon, default_cloud_init_grows_root_fs)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, configure(_, _, _))
        .WillOnce(Invoke([](const std::string& name, YAML::Node& meta_config, YAML::Node& user_config) {
            EXPECT_THAT(user_config, YAMLNodeContainsMap("growpart"));

            if (user_config["growpart"])
            {
                auto const& growpart_stanza = user_config["growpart"];

                EXPECT_THAT(growpart_stanza, YAMLNodeContainsString("mode", "auto"));
                EXPECT_THAT(growpart_stanza, YAMLNodeContainsStringArray("devices", std::vector<std::string>({"/"})));
                EXPECT_THAT(growpart_stanza, YAMLNodeContainsString("ignore_growroot_disabled", "false"));
            }
        }));

    send_command({"launch"});
}

namespace
{
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
} // namespace

TEST_F(Daemon, adds_ssh_keys_to_cloud_init_config)
{
    auto mock_factory = use_a_mock_vm_factory();
    std::string expected_key{"thisitnotansshkeyactually"};
    config_builder.ssh_key_provider = std::make_unique<DummyKeyProvider>(expected_key);
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, configure(_, _, _))
        .WillOnce(Invoke([&expected_key](const std::string& name, YAML::Node& meta_config, YAML::Node& user_config) {
            ASSERT_THAT(user_config, YAMLNodeContainsSequence("ssh_authorized_keys"));
            auto const& ssh_keys_stanza = user_config["ssh_authorized_keys"];
            EXPECT_THAT(ssh_keys_stanza, YAMLNodeContainsSubString(expected_key));
        }));

    send_command({"launch"});
}

namespace
{
struct MinSpaceRespectedSuite : public Daemon, public WithParamInterface<std::string>
{
};

struct MinSpaceViolatedSuite : public Daemon, public WithParamInterface<std::string>
{
};

TEST_P(MinSpaceRespectedSuite, accepts_launch_with_enough_explicit_memory)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _));
    send_command({"launch", "--mem", GetParam()});
}

TEST_P(MinSpaceRespectedSuite, accepts_launch_with_enough_explicit_disk)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _));
    send_command({"launch", "--disk", GetParam()});
}

TEST_P(MinSpaceViolatedSuite, refuses_launch_with_memory_below_threshold)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).Times(0); // expect no call
    send_command({"launch", "--mem", GetParam()}, stream);
    EXPECT_THAT(stream.str(), AllOf(HasSubstr("fail"), HasSubstr("memory"), HasSubstr("minimum")));
}

TEST_P(MinSpaceViolatedSuite, refuses_launch_with_disk_below_threshold)
{
    auto mock_factory = use_a_mock_vm_factory();
    mp::Daemon daemon{config_builder.build()};

    std::stringstream stream;

    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).Times(0); // expect no call
    send_command({"launch", "--disk", GetParam()}, stream);
    EXPECT_THAT(stream.str(), AllOf(HasSubstr("fail"), HasSubstr("memory"), HasSubstr("minimum")));
}

INSTANTIATE_TEST_SUITE_P(Daemon, MinSpaceRespectedSuite, Values("1024m", "2Gb", "987654321"));
INSTANTIATE_TEST_SUITE_P(Daemon, MinSpaceViolatedSuite, Values("0", "0B", "0GB", "123B", "42kb", "100"));

} // namespace
