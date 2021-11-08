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
void set_logger();
void set_logger(multipass::logging::Level verbosity); // full param qualification makes sure msvc is happy
void pre_setup();
void post_setup();
}
} // namespace multipass
#endif // MULTIPASS_CLIENT_COMMON_H
