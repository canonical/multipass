/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#include "daemon_test_fixture.h"
#include "common.h"
#include "file_operations.h"
#include "mock_cert_provider.h"
#include "mock_server_reader_writer.h"
#include "mock_standard_paths.h"
#include "stub_cert_store.h"
#include "stub_image_host.h"
#include "stub_logger.h"
#include "stub_ssh_key_provider.h"
#include "stub_terminal.h"
#include "stub_virtual_machine_factory.h"
#include "stub_vm_blueprint_provider.h"
#include "stub_vm_image_vault.h"

#include <src/client/cli/client.h>
#include <src/platform/update/disabled_update_prompt.h>

#include <multipass/auto_join_thread.h>
#include <multipass/cli/argparser.h>
#include <multipass/cli/client_common.h>
#include <multipass/cli/command.h>

#include <chrono>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
class TestCreate final : public multipass::cmd::Command
{
public:
    using Command::Command;
    multipass::ReturnCode run(multipass::ArgParser* parser) override
    {
        auto on_success = [](multipass::CreateReply& /*reply*/) { return multipass::ReturnCode::Ok; };
        auto on_failure = [this](grpc::Status& status) {
            multipass::CreateError create_error;
            create_error.ParseFromString(status.error_details());
            const auto errors = create_error.error_codes();

            cerr << "failed: " << status.error_message();
            if (errors.size() == 1)
            {
                const auto& error = errors[0];
                if (error == multipass::CreateError::INVALID_DISK_SIZE)
                    cerr << "disk";
                else if (error == multipass::CreateError::INVALID_MEM_SIZE)
                    cerr << "memory";
                else
                    cerr << "?";
            }

            return multipass::ReturnCode::CommandFail;
        };

        auto streaming_callback =
            [this](mp::CreateReply& reply,
                   grpc::ClientReaderWriterInterface<mp::CreateRequest, mp::CreateReply>* client) {
                cout << reply.create_message() << std::endl;
            };

        auto ret = parse_args(parser);
        return ret == multipass::ParseCode::Ok
                   ? dispatch(&mp::Rpc::StubInterface::create, request, on_success, on_failure, streaming_callback)
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
    multipass::ParseCode parse_args(multipass::ArgParser* parser)
    {
        parser->addPositionalArgument("image", "", "");
        QCommandLineOption diskOption("disk", "", "disk", "");
        QCommandLineOption memOption("memory", "", "memory", "");
        parser->addOptions({diskOption, memOption});

        auto status = parser->commandParse(this);
        if (status == multipass::ParseCode::Ok)
        {
            if (!parser->positionalArguments().isEmpty())
                request.set_image(parser->positionalArguments().first().toStdString());

            if (parser->isSet(memOption))
                request.set_mem_size(parser->value(memOption).toStdString());

            if (parser->isSet(diskOption))
                request.set_disk_space(parser->value(diskOption).toStdString());
        }

        return status;
    }

    multipass::CreateRequest request;
};

class TestGet final : public mp::cmd::Command
{
public:
    using Command::Command;
    mp::ReturnCode run(mp::ArgParser* parser) override
    {
        std::string val;
        auto on_success = [&val](mp::GetReply& reply) {
            val = reply.value();
            return mp::ReturnCode::Ok;
        };

        auto on_failure = [this](grpc::Status& status) {
            return mp::cmd::standard_failure_handler_for(name(), cerr, status);
        };

        if (auto parse_result = parse_args(parser); parse_result == mp::ParseCode::Ok)
        {
            auto ret = dispatch(&mp::Rpc::StubInterface::get, request, on_success, on_failure);
            cout << fmt::format("{}={}", request.key(), val);
            return ret;
        }
        else
            return parser->returnCodeFrom(parse_result);
    }

    std::string name() const override
    {
        return "test_get";
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
    mp::ParseCode parse_args(mp::ArgParser* parser)
    {
        parser->addPositionalArgument("key", "key of the setting to get");

        auto status = parser->commandParse(this);
        if (status == multipass::ParseCode::Ok)
        {
            const auto args = parser->positionalArguments();
            if (args.count() == 1)
                request.set_key(args.at(0).toStdString());
            else
                status = mp::ParseCode::CommandLineError;
        }

        return status;
    }

    mp::GetRequest request;
};

class TestSet final : public mp::cmd::Command
{
public:
    using Command::Command;

    mp::ReturnCode run(mp::ArgParser* parser) override
    {
        auto on_success = [](mp::SetReply&) { return mp::ReturnCode::Ok; };
        auto on_failure = [this](grpc::Status& status) {
            return mp::cmd::standard_failure_handler_for(name(), cerr, status);
        };

        if (auto parse_result = parse_args(parser); parse_result == mp::ParseCode::Ok)
            return dispatch(&mp::Rpc::StubInterface::set, request, on_success, on_failure);
        else
            return parser->returnCodeFrom(parse_result);
    }

    std::string name() const override
    {
        return "test_set";
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
    mp::ParseCode parse_args(mp::ArgParser* parser)
    {
        parser->addPositionalArgument("key", "setting key");
        parser->addPositionalArgument("val", "setting value");

        auto status = parser->commandParse(this);
        if (status == mp::ParseCode::Ok)
        {
            const auto& args = parser->positionalArguments();
            if (args.count() == 2)
            {
                request.set_key(args.at(0).toStdString());
                request.set_val(args.at(1).toStdString());
            }
            else
                status = mp::ParseCode::CommandLineError;
        }

        return status;
    }

    mp::SetRequest request;
};

class TestKeys final : public mp::cmd::Command
{
public:
    using Command::Command;

    mp::ReturnCode run(mp::ArgParser* parser) override
    {
        auto on_success = [](mp::KeysReply&) { return mp::ReturnCode::Ok; };
        auto on_failure = [this](grpc::Status& status) {
            return mp::cmd::standard_failure_handler_for(name(), cerr, status);
        };

        if (auto parse_result = parse_args(parser); parse_result == mp::ParseCode::Ok)
            return dispatch(&mp::Rpc::StubInterface::keys, request, on_success, on_failure);
        else
            return parser->returnCodeFrom(parse_result);
    }

    std::string name() const override
    {
        return "test_keys";
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
    mp::ParseCode parse_args(mp::ArgParser* parser)
    {
        return parser->commandParse(this) == mp::ParseCode::Ok ? mp::ParseCode::Ok : mp::ParseCode::CommandLineError;
    }

    mp::KeysRequest request;
};

class TestClient : public multipass::Client
{
public:
    explicit TestClient(multipass::ClientConfig& context) : multipass::Client{context}
    {
        add_command<TestCreate>();
        add_command<TestKeys>();
        add_command<TestGet>();
        add_command<TestSet>();
        sort_commands();
    }
};

} // namespace

mpt::DaemonTestFixture::DaemonTestFixture()
{
    config_builder.server_address = server_address;
    config_builder.cache_directory = cache_dir.path();
    config_builder.data_directory = data_dir.path();
    config_builder.vault = std::make_unique<StubVMImageVault>();
    config_builder.factory = std::make_unique<StubVirtualMachineFactory>();
    config_builder.image_hosts.push_back(std::make_unique<StubVMImageHost>());
    config_builder.ssh_key_provider = std::make_unique<StubSSHKeyProvider>();
    config_builder.cert_provider = std::make_unique<MockCertProvider>();
    config_builder.client_cert_store = std::make_unique<StubCertStore>();
    config_builder.logger = std::make_unique<StubLogger>();
    config_builder.update_prompt = std::make_unique<DisabledUpdatePrompt>();
    config_builder.blueprint_provider = std::make_unique<StubVMBlueprintProvider>();
}

void mpt::DaemonTestFixture::SetUp()
{
    EXPECT_CALL(MockStandardPaths::mock_instance(), locate(_, _, _))
        .Times(AnyNumber()); // needed to allow general calls once we have added the specific expectation below
    EXPECT_CALL(MockStandardPaths::mock_instance(), locate(_, match_qstring(EndsWith("settings.json")), _))
        .Times(AnyNumber())
        .WillRepeatedly(Return("")); /* Avoid writing to Windows Terminal settings. We use an "expectation" so that
                                        it gets reset at the end of each test (by VerifyAndClearExpectations) */
}

mpt::MockVirtualMachineFactory* mpt::DaemonTestFixture::use_a_mock_vm_factory()
{
    auto mock_factory = std::make_unique<NiceMock<MockVirtualMachineFactory>>();
    auto mock_factory_ptr = mock_factory.get();

    ON_CALL(*mock_factory_ptr, fetch_type()).WillByDefault(Return(FetchType::ImageOnly));

    ON_CALL(*mock_factory_ptr, create_virtual_machine).WillByDefault([](const auto&, auto&) {
        return std::make_unique<StubVirtualMachine>();
    });

    ON_CALL(*mock_factory_ptr, prepare_source_image(_)).WillByDefault(ReturnArg<0>());

    ON_CALL(*mock_factory_ptr, get_backend_version_string()).WillByDefault(Return("mock-1234"));

    ON_CALL(*mock_factory_ptr, networks())
        .WillByDefault(Return(std::vector<NetworkInterfaceInfo>{{"eth0", "ethernet", "wired adapter"},
                                                                {"wlan0", "wi-fi", "wireless adapter"}}));

    config_builder.factory = std::move(mock_factory);
    return mock_factory_ptr;
}

void mpt::DaemonTestFixture::send_command(const std::vector<std::string>& command, std::ostream& cout,
                                          std::ostream& cerr, std::istream& cin)
{
    send_commands({command}, cout, cerr, cin);
}

// "commands" is a vector of commands that includes necessary positional arguments, ie,
// "start foo"
void mpt::DaemonTestFixture::send_commands(std::vector<std::vector<std::string>> commands, std::ostream& cout,
                                           std::ostream& cerr, std::istream& cin)
{
    // Commands need to be sent from a thread different from that the QEventLoop is on.
    // Event loop is started/stopped to ensure all signals are delivered
    AutoJoinThread t([this, &commands, &cout, &cerr, &cin] {
        StubTerminal term(cout, cerr, cin);

        std::unique_ptr<CertProvider> cert_provider;
        cert_provider = std::make_unique<MockCertProvider>();

        ClientConfig client_config{server_address, std::move(cert_provider), &term};
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

        // Commands not using RPC do not block in the "t" thread. This means that there will be a deadlock if
        // loop.exec() is called after loop.quit(). The following check avoids this scenario, by making the
        // thread sleep until the loop is running.
        while (!loop.isRunning())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        loop.quit();
    });
    loop.exec();
}

int mpt::DaemonTestFixture::total_lines_of_output(std::stringstream& output)
{
    int count{0};
    std::string line;

    while (std::getline(output, line))
    {
        count++;
    }

    return count;
}

std::string mpt::DaemonTestFixture::fake_json_contents(const std::string& default_mac,
                                                       const std::vector<mp::NetworkInterface>& extra_ifaces,
                                                       const std::unordered_map<std::string, mp::VMMount>& mounts)
{
    QString contents("{\n"
                     "    \"real-zebraphant\": {\n"
                     "        \"deleted\": false,\n"
                     "        \"disk_space\": \"5368709120\",\n"
                     "        \"extra_interfaces\": [\n");

    QStringList array_elements;
    for (auto extra_interface : extra_ifaces)
    {
        array_elements += QString::fromStdString(fmt::format("            {{\n"
                                                             "                \"auto_mode\": {},\n"
                                                             "                \"id\": \"{}\",\n"
                                                             "                \"mac_address\": \"{}\"\n"
                                                             "            }}\n",
                                                             extra_interface.auto_mode, extra_interface.id,
                                                             extra_interface.mac_address));
    }
    contents += array_elements.join(',');

    contents += QString::fromStdString(fmt::format("        ],\n"
                                                   "        \"mac_addr\": \"{}\",\n"
                                                   "        \"mem_size\": \"1073741824\",\n"
                                                   "        \"metadata\": {{\n"
                                                   "            \"arguments\": [\n"
                                                   "                \"many\",\n"
                                                   "                \"arguments\"\n"
                                                   "            ],\n"
                                                   "            \"machine_type\": \"dmc-de-lorean\"\n"
                                                   "        }},\n"
                                                   "        \"mounts\": [\n",
                                                   default_mac));

    QStringList mount_array_elements;
    for (const auto& mount_pair : mounts)
    {
        const auto& mountpoint = mount_pair.first;
        const auto& mount = mount_pair.second;

        QString mount_element;

        mount_element += QString::fromStdString(fmt::format("            {{\n"
                                                            "                \"gid_mappings\": ["));

        QStringList gid_array_elements;
        for (const auto& gid_pair : mount.gid_mappings)
        {
            gid_array_elements += QString::fromStdString(fmt::format("\n                    {{\n"
                                                                     "                        \"host_gid\": {},\n"
                                                                     "                        \"instance_gid\": {}\n"
                                                                     "                    }}",
                                                                     gid_pair.first, gid_pair.second));
        }
        mount_element += gid_array_elements.join(',');

        mount_element += QString::fromStdString(fmt::format("\n                ],\n"
                                                            "                \"source_path\": \"{}\",\n"
                                                            "                \"target_path\": \"{}\",\n",
                                                            mount.source_path, mountpoint));
        mount_element += QString::fromStdString(fmt::format("                \"mount_type\": {},\n"
                                                            "                \"uid_mappings\": [",
                                                            mount.mount_type));

        QStringList uid_array_elements;
        for (const auto& uid_pair : mount.uid_mappings)
        {
            uid_array_elements += QString::fromStdString(fmt::format("\n                    {{\n"
                                                                     "                        \"host_uid\": {},\n"
                                                                     "                        \"instance_uid\": {}\n"
                                                                     "                    }}",
                                                                     uid_pair.first, uid_pair.second));
        }
        mount_element += uid_array_elements.join(',');

        mount_element += QString::fromStdString(fmt::format("\n                ]\n"
                                                            "            }}"));

        mount_array_elements += mount_element;
    }

    contents += mount_array_elements.join(",\n");

    contents += QString::fromStdString(fmt::format("\n        ],\n"
                                                   "        \"num_cores\": 1,\n"
                                                   "        \"ssh_username\": \"ubuntu\",\n"
                                                   "        \"state\": 2\n"
                                                   "    }}\n"
                                                   "}}"));

    return contents.toStdString();
}

std::pair<std::unique_ptr<mpt::TempDir>, QString>
mpt::DaemonTestFixture::plant_instance_json(const std::string& contents)
{
    auto temp_dir = std::make_unique<TempDir>();
    QString filename(temp_dir->path() + "/multipassd-vm-instances.json");

    make_file_with_content(filename, contents);

    return {std::move(temp_dir), filename};
}

template <typename R>
bool mpt::DaemonTestFixture::is_ready(std::future<R> const& f)
{
    // 5 seconds should be plenty of time for the work to be complete
    return f.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
}

template <typename DaemonSlotPtr, typename Request, typename Server>
grpc::Status mpt::DaemonTestFixture::call_daemon_slot(Daemon& daemon, DaemonSlotPtr slot, const Request& request,
                                                      Server&& server)
{
    std::promise<grpc::Status> status_promise;
    auto status_future = status_promise.get_future();

    auto thread = QThread::create([&daemon, slot, &request, &server, &status_promise] {
        QEventLoop loop;
        (daemon.*slot)(&request, &server, &status_promise);
        loop.exec();
    });

    thread->start();

    EXPECT_TRUE(is_ready(status_future));

    thread->quit();

    return status_future.get();
}

template bool mpt::DaemonTestFixture::is_ready(std::future<grpc::Status> const&);

template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(const mp::AuthenticateRequest*,
                         grpc::ServerReaderWriterInterface<mp::AuthenticateReply, mp::AuthenticateRequest>*,
                         std::promise<grpc::Status>*),
    const mp::AuthenticateRequest&,
    StrictMock<mpt::MockServerReaderWriter<mp::AuthenticateReply, mp::AuthenticateRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::VersionRequest const*,
                         grpc::ServerReaderWriterInterface<mp::VersionReply, mp::VersionRequest>*,
                         std::promise<grpc::Status>*),
    mp::VersionRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::VersionReply, mp::VersionRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(const mp::ListRequest*, grpc::ServerReaderWriterInterface<mp::ListReply, mp::ListRequest>*,
                         std::promise<grpc::Status>*),
    mp::ListRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::ListReply, mp::ListRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::KeysRequest const*, grpc::ServerReaderWriterInterface<mp::KeysReply, mp::KeysRequest>*,
                         std::promise<grpc::Status>*),
    mp::KeysRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::KeysReply, mp::KeysRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::KeysRequest const*, grpc::ServerReaderWriterInterface<mp::KeysReply, mp::KeysRequest>*,
                         std::promise<grpc::Status>*),
    mp::KeysRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::KeysReply, mp::KeysRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::GetRequest const*, grpc::ServerReaderWriterInterface<mp::GetReply, mp::GetRequest>*,
                         std::promise<grpc::Status>*),
    mp::GetRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::GetReply, mp::GetRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::GetRequest const*, grpc::ServerReaderWriterInterface<mp::GetReply, mp::GetRequest>*,
                         std::promise<grpc::Status>*),
    mp::GetRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::GetReply, mp::GetRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::SetRequest const*, grpc::ServerReaderWriterInterface<mp::SetReply, mp::SetRequest>*,
                         std::promise<grpc::Status>*),
    mp::SetRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::SetRequest const*, grpc::ServerReaderWriterInterface<mp::SetReply, mp::SetRequest>*,
                         std::promise<grpc::Status>*),
    mp::SetRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::NetworksRequest const*,
                         grpc::ServerReaderWriterInterface<mp::NetworksReply, mp::NetworksRequest>*,
                         std::promise<grpc::Status>*),
    mp::NetworksRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::NetworksReply, mp::NetworksRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::NetworksRequest const*,
                         grpc::ServerReaderWriterInterface<mp::NetworksReply, mp::NetworksRequest>*,
                         std::promise<grpc::Status>*),
    mp::NetworksRequest const&, NiceMock<mpt::MockServerReaderWriter<mp::NetworksReply, mp::NetworksRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::PurgeRequest const*, grpc::ServerReaderWriterInterface<mp::PurgeReply, mp::PurgeRequest>*,
                         std::promise<grpc::Status>*),
    mp::PurgeRequest const&, NiceMock<mpt::MockServerReaderWriter<mp::PurgeReply, mp::PurgeRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(const mp::MountRequest*, grpc::ServerReaderWriterInterface<mp::MountReply, mp::MountRequest>*,
                         std::promise<grpc::Status>*),
    const mp::MountRequest&, StrictMock<mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(const mp::UmountRequest*,
                         grpc::ServerReaderWriterInterface<mp::UmountReply, mp::UmountRequest>*,
                         std::promise<grpc::Status>*),
    const mp::UmountRequest&, StrictMock<mpt::MockServerReaderWriter<mp::UmountReply, mp::UmountRequest>>&&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(mp::LaunchRequest const*,
                         grpc::ServerReaderWriterInterface<mp::LaunchReply, mp::LaunchRequest>*,
                         std::promise<grpc::Status>*),
    mp::LaunchRequest const&, StrictMock<mpt::MockServerReaderWriter<mp::LaunchReply, mp::LaunchRequest>>&);
template grpc::Status mpt::DaemonTestFixture::call_daemon_slot(
    mp::Daemon&,
    void (mp::Daemon::*)(const mp::StartRequest*, grpc::ServerReaderWriterInterface<mp::StartReply, mp::StartRequest>*,
                         std::promise<grpc::Status>*),
    const mp::StartRequest&, StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>&&);
