/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include "../cli/cmd/common_cli.h"

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/platform.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <fmt/ostream.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>

#include <grpc++/grpc++.h>

#include <QString>

#include <iostream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
std::stringstream null_stream;

mp::ReturnCode return_code_for(const grpc::StatusCode& code)
{
    return code == grpc::StatusCode::UNAVAILABLE ? mp::ReturnCode::DaemonFail : mp::ReturnCode::CommandFail;
}
} // namespace

mp::ReturnCode mp::cmd::standard_failure_handler_for(const std::string& command, std::ostream& cerr,
                                                     const grpc::Status& status, const std::string& error_details)
{
    fmt::print(cerr, "{} failed: {}\n{}", command, status.error_message(),
               !error_details.empty()
                   ? fmt::format("{}\n", error_details)
                   : !status.error_details().empty() ? fmt::format("{}\n", status.error_details()) : "");

    return return_code_for(status.error_code());
}

bool mp::cmd::update_available(const mp::UpdateInfo& update_info)
{
    return update_info.version() != "";
}

std::shared_ptr<grpc::Channel> mp::client::make_channel(const std::string& server_address,
                                                        mp::RpcConnectionType conn_type,
                                                        mp::CertProvider& cert_provider)
{
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (conn_type == mp::RpcConnectionType::ssl)
    {
        auto opts = grpc::SslCredentialsOptions();
        opts.server_certificate_request = GRPC_SSL_REQUEST_SERVER_CERTIFICATE_BUT_DONT_VERIFY;
        opts.pem_cert_chain = cert_provider.PEM_certificate();
        opts.pem_private_key = cert_provider.PEM_signing_key();
        creds = grpc::SslCredentials(opts);
    }
    else if (conn_type == mp::RpcConnectionType::insecure)
    {
        creds = grpc::InsecureChannelCredentials();
    }
    else
    {
        throw std::runtime_error("Unknown connection type");
    }
    return grpc::CreateChannel(server_address, creds);
}

std::string mp::client::get_server_address()
{
    const auto address = qgetenv("MULTIPASS_SERVER_ADDRESS").toStdString();
    if (!address.empty())
    {
        mp::utils::validate_server_address(address);
        return address;
    }

    return mp::platform::default_server_address();
}

std::unique_ptr<mp::SSLCertProvider> mp::client::get_cert_provider()
{
    auto data_dir = StandardPaths::instance().writableLocation(StandardPaths::AppDataLocation);
    auto client_cert_dir = mp::utils::make_dir(data_dir, "client-certificate");
    return std::make_unique<mp::SSLCertProvider>(client_cert_dir);
}

void mp::client::set_logger()
{
    set_logger(mpl::Level::info);
}

void mp::client::set_logger(mpl::Level verbosity)
{
    mpl::set_logger(std::make_shared<mpl::StandardLogger>(verbosity));
}

void mp::client::preliminary_setup()
{
    try
    {
        platform::setup_gui_autostart_prerequisites();
    }
    catch (AutostartSetupException& e)
    {
        mpl::log(mpl::Level::error, "client", fmt::format("Failed to set up autostart prerequisites: {}", e.what()));
        mpl::log(mpl::Level::debug, "client", e.get_detail());
    }
}

void mp::client::final_adjustments(Rpc::Stub& stub, const mp::ArgParser* parser, std::ostream& cout, std::ostream& cerr)
{
    platform::sync_winterm_profiles();

    if (mp::Settings::instance().get_as<bool>(mp::lxc_key))
    {
        ListReply reply;
        ListRequest request;

        grpc::ClientContext context;
        std::unique_ptr<grpc::ClientReader<ListReply>> reader = stub.list(&context, request);

        while (reader->Read(&reply))
        {
            if (!reply.log_line().empty())
                std::cerr << reply.log_line() << "\n";
        }

        auto status = reader->Finish();

        if (status.ok())
        {
            for (const auto& instance : reply.instances())
            {
                if (!instance.ipv4().empty() &&
                    mp::cmd::run_cmd(
                        {"multipass", "exec", QString::fromStdString(instance.name()), "--", "which", "lxd"}, parser,
                        null_stream, null_stream) == mp::ReturnCode::Ok)
                {
                    mp::cmd::run_cmd({"multipass", "exec", QString::fromStdString(instance.name()), "--", "sudo",
                                      "apt-get", "--quiet", "--quiet", "update"},
                                     parser, cout, cerr);
                    mp::cmd::run_cmd({"multipass", "exec", QString::fromStdString(instance.name()), "--", "sudo",
                                      "apt-get", "--quiet", "--quiet", "--yes", "install", "zfsutils-linux"},
                                     parser, cout, cerr);
                    mp::cmd::run_cmd({"multipass", "exec", QString::fromStdString(instance.name()), "--", "sudo", "lxd",
                                      "init", "--auto", "--storage-backend", "zfs", "--network-address", "0",
                                      "--trust-password", "multipass"},
                                     parser, cout, cerr);
                    auto lxc =
                        mp::platform::make_process("lxc", {"remote", "add", QString::fromStdString(instance.name()),
                                                           QString::fromStdString(instance.ipv4()),
                                                           "--accept-certificate", "--password=multipass"});
                    auto lxc_state = lxc->execute();

                    if (!lxc_state.completed_successfully())
                    {
                        throw std::runtime_error(fmt::format("Internal error: lxc failed ({}) with output:\n{}",
                                                             lxc_state.failure_message(),
                                                             lxc->read_all_standard_error()));
                    }
                }
            }
        }
        else if (status.error_code() != grpc::StatusCode::UNAVAILABLE)
        {
            throw std::runtime_error("Failed to list instances for lxc integration...");
        }
        else
        {
            throw std::runtime_error("Failed to talk to the Multipass daemon...");
        }
    }
}
