/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_COMMON_CLI_H
#define MULTIPASS_COMMON_CLI_H

#include <multipass/cli/return_codes.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <QString>

using RpcMethod = multipass::Rpc::Stub;

namespace multipass
{
class ArgParser;
class Formatter;

namespace cmd
{
const QString all_option_name{"all"};
const QString format_option_name{"format"};

ParseCode check_for_name_and_all_option_conflict(ArgParser* parser, std::ostream& cerr);
InstanceNames add_instance_names(ArgParser* parser);
ParseCode handle_format_option(ArgParser* parser, Formatter** chosen_formatter, std::ostream& cerr);
std::string instance_action_message_for(const InstanceNames& instance_names, const std::string& action_name);
ReturnCode standard_failure_handler_for(const std::string& command, std::ostream& cerr, const grpc::Status& status,
                                        const std::string& error_details = std::string());
} // namespace cmd
} // namespace multipass

#endif // MULTIPASS_COMMON_CLI_H
