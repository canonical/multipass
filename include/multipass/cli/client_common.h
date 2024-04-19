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

#ifndef MULTIPASS_CLIENT_COMMON_H
#define MULTIPASS_CLIENT_COMMON_H

#include <grpcpp/grpcpp.h>

#include <multipass/cert_provider.h>
#include <multipass/cli/client_platform.h>
#include <multipass/cli/return_codes.h>
#include <multipass/console.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/ssl_cert_provider.h>

#include <memory>
#include <regex>
#include <string>

namespace multipass
{
const QString common_client_cert_dir{"/multipass-client-certificate"};
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

template <typename Request, typename Reply>
void handle_password(grpc::ClientReaderWriterInterface<Request, Reply>* client, Terminal* term)
{
    Request request;
    request.set_password(MP_CLIENT_PLATFORM.get_password(term));
    client->Write(request);
}
}

namespace client
{
QString persistent_settings_filename();
void register_global_settings_handlers();
std::shared_ptr<grpc::Channel> make_channel(const std::string& server_address, const CertProvider& cert_provider);
std::string get_server_address();
std::unique_ptr<SSLCertProvider> get_cert_provider();
void set_logger();
void set_logger(multipass::logging::Level verbosity); // full param qualification makes sure msvc is happy
void post_setup();
const std::regex yes_answer{"y|yes", std::regex::icase | std::regex::optimize};
const std::regex no_answer{"n|no", std::regex::icase | std::regex::optimize};
}
} // namespace multipass
#endif // MULTIPASS_CLIENT_COMMON_H
