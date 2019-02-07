/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/cli/client_common.h>

#include <fmt/ostream.h>

namespace mp = multipass;

namespace
{
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
