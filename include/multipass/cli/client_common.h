/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_CLIENT_COMMON_H
#define MULTIPASS_CLIENT_COMMON_H

#include <grpcpp/grpcpp.h>

#include <multipass/cert_provider.h>
#include <multipass/cli/return_codes.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/rpc_connection_type.h>
#include <multipass/ssl_cert_provider.h>

#include <memory>
#include <string>

namespace multipass
{
const QString common_client_cert_dir{"/multipass-client-certificate"};
const QString gui_client_cert_dir{"/multipass-gui/client-certificate"};
const QString cli_client_cert_dir{"/multipass/client-certificate"};
const QString client_cert_prefix{"multipass_cert"};
const QString cert_file_suffix{".pem"};
const QString key_file_suffix{"_key.pem"};
const QString client_cert_file{client_cert_prefix + cert_file_suffix};
const QString client_key_file{client_cert_prefix + key_file_suffix};

namespace logging
{
enum class Level : int; // Fwd decl
}

namespace cmd
{
multipass::ReturnCode standard_failure_handler_for(const std::string& command, std::ostream& cerr,
                                                   const grpc::Status& status,
                                                   const std::string& error_details = std::string());
bool update_available(const UpdateInfo& update_info);
std::string update_notice(const multipass::UpdateInfo& update_info);
}

namespace client
{
std::shared_ptr<grpc::Channel> make_secure_channel(const std::string& server_address, CertProvider* cert_provider);
std::shared_ptr<grpc::Channel> make_insecure_channel(const std::string& server_address);
std::string get_server_address();
std::unique_ptr<SSLCertProvider> get_cert_provider();
void set_logger();
void set_logger(multipass::logging::Level verbosity); // full param qualification makes sure msvc is happy
void pre_setup();
void post_setup();
}
} // namespace multipass
#endif // MULTIPASS_CLIENT_COMMON_H
