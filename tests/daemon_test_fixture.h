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

#ifndef MULTIPASS_DAEMON_TEST_FIXTURE_H
#define MULTIPASS_DAEMON_TEST_FIXTURE_H

#include "common.h"
#include "mock_cert_provider.h"
#include "mock_standard_paths.h"
#include "mock_virtual_machine_factory.h"
#include "stub_cert_store.h"
#include "stub_certprovider.h"
#include "stub_image_host.h"
#include "stub_logger.h"
#include "stub_ssh_key_provider.h"
#include "stub_terminal.h"
#include "stub_virtual_machine_factory.h"
#include "stub_vm_image_vault.h"
#include "stub_vm_workflow_provider.h"
#include "temp_dir.h"

#include <src/client/cli/client.h>
#include <src/daemon/daemon.h>
#include <src/daemon/daemon_config.h>
#include <src/daemon/daemon_rpc.h>
#include <src/platform/update/disabled_update_prompt.h>

#include <multipass/auto_join_thread.h>
#include <multipass/cli/argparser.h>
#include <multipass/cli/client_common.h>
#include <multipass/cli/command.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <chrono>
#include <memory>

using namespace testing;
namespace mp = multipass;

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

        auto streaming_callback = [this](multipass::CreateReply& reply) {
            cout << reply.create_message() << std::endl;
        };

        auto ret = parse_args(parser);
        return ret == multipass::ParseCode::Ok
                   ? dispatch(&multipass::Rpc::Stub::create, request, on_success, on_failure, streaming_callback)
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
    multipass::ParseCode parse_args(multipass::ArgParser* parser) override
    {
        QCommandLineOption diskOption("disk", "", "disk", "");
        QCommandLineOption memOption("mem", "", "mem", "");
        parser->addOptions({diskOption, memOption});

        auto status = parser->commandParse(this);
        if (status == multipass::ParseCode::Ok)
        {
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
            auto ret = dispatch(&mp::Rpc::Stub::get, request, on_success, on_failure);
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
    mp::ParseCode parse_args(mp::ArgParser* parser) override
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

class TestClient : public multipass::Client
{
public:
    explicit TestClient(multipass::ClientConfig& context) : multipass::Client{context}
    {
        add_command<TestCreate>();
        add_command<TestGet>();
        sort_commands();
    }
};

} // namespace

namespace multipass
{
namespace test
{
template <typename R>
bool is_ready(std::future<R> const& f)
{
    return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template <typename W>
class MockServerWriter : public grpc::ServerWriterInterface<W>
{
public:
    MOCK_METHOD0(SendInitialMetadata, void());
    MOCK_METHOD2_T(Write, bool(const W& msg, grpc::WriteOptions options));
};

/**
 * Helper function to call one of the <em>daemon slots</em> that ultimately handle RPC requests
 *  (e.g. @c mp::Daemon::get). It takes care of promise/future boilerplate. This will generally be given a
 *  @c mpt::MockServerWriter, which can be used to verify replies.
 * @tparam DaemonSlotPtr The method pointer type for the provided slot. Example:
 *  @code
 *  void (mp::Daemon::*)(const mp::GetRequest *, grpc::ServerWriterInterface<mp::GetReply> *,
 *                       std::promise<grpc::Status> *)>
 *  @endcode
 * @tparam Request The request type that the provided slot expects. Example: @c mp::GetRequest
 * @tparam Server The concrete @c grpc::ServerWriterInterface type that the provided slot expects. The template needs to
 *  be instantiated with the correct reply type. Example: <tt> grpc::ServerWriterInterface\<mp\::GetReply\> </tt>
 * @param daemon The daemon object to call the slot on.
 * @param slot A pointer to the daemon slot method that should be called.
 * @param request The request to call the slot with.
 * @param server The concrete @c grpc::ServerWriterInterface to call the slot with (see doc on Server typename). This
 *  will generally be a @c mpt::MockServerWriter. Notice that this is a <em>universal reference</em>, so it will bind
 *  to both lvalue and rvalue references.
 * @return The @c grpc::Status that is produced by the slot
 */
template <typename DaemonSlotPtr, typename Request, typename Server>
grpc::Status call_daemon_slot(Daemon& daemon, DaemonSlotPtr slot, const Request& request, Server&& server)
{
    std::promise<grpc::Status> status_promise;
    auto status_future = status_promise.get_future();

    (daemon.*slot)(&request, &server, &status_promise);

    EXPECT_TRUE(is_ready(status_future));
    return status_future.get();
}

struct DaemonTestFixture : public ::Test
{
    DaemonTestFixture()
    {
        config_builder.server_address = server_address;
        config_builder.cache_directory = cache_dir.path();
        config_builder.data_directory = data_dir.path();
        config_builder.vault = std::make_unique<StubVMImageVault>();
        config_builder.factory = std::make_unique<StubVirtualMachineFactory>();
        config_builder.image_hosts.push_back(std::make_unique<StubVMImageHost>());
        config_builder.ssh_key_provider = std::make_unique<StubSSHKeyProvider>();
        config_builder.cert_provider = std::make_unique<StubCertProvider>();
        config_builder.client_cert_store = std::make_unique<StubCertStore>();
        config_builder.connection_type = RpcConnectionType::insecure;
        config_builder.logger = std::make_unique<StubLogger>();
        config_builder.update_prompt = std::make_unique<DisabledUpdatePrompt>();
        config_builder.workflow_provider = std::make_unique<StubVMWorkflowProvider>();
    }

    void SetUp() override
    {
        EXPECT_CALL(MockStandardPaths::mock_instance(), locate(_, _, _))
            .Times(AnyNumber()); // needed to allow general calls once we have added the specific expectation below
        EXPECT_CALL(MockStandardPaths::mock_instance(), locate(_, match_qstring(EndsWith("settings.json")), _))
            .Times(AnyNumber())
            .WillRepeatedly(Return("")); /* Avoid writing to Windows Terminal settings. We use an "expectation" so that
                                            it gets reset at the end of each test (by VerifyAndClearExpectations) */
    }

    MockVirtualMachineFactory* use_a_mock_vm_factory()
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
        AutoJoinThread t([this, &commands, &cout, &cerr, &cin] {
            StubTerminal term(cout, cerr, cin);

            std::unique_ptr<CertProvider> cert_provider;
            if (config_builder.connection_type == RpcConnectionType::ssl)
            {
                cert_provider = std::make_unique<MockCertProvider>();
            }
            else
            {
                cert_provider = std::make_unique<StubCertProvider>();
            }

            ClientConfig client_config{server_address, config_builder.connection_type, std::move(cert_provider), &term};
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

    int total_lines_of_output(std::stringstream& output)
    {
        int count{0};
        std::string line;

        while (std::getline(output, line))
        {
            count++;
        }

        return count;
    }

#ifdef MULTIPASS_PLATFORM_WINDOWS
    std::string server_address{"localhost:50051"};
#else
    std::string server_address{"unix:/tmp/test-multipassd.socket"};
#endif
    QEventLoop loop; // needed as signal/slots used internally by mp::Daemon
    TempDir cache_dir;
    TempDir data_dir;
    DaemonConfigBuilder config_builder;
    inline static std::stringstream trash_stream{}; // this may have contents (that we don't care about)
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_DAEMON_TEST_FIXTURE_H
