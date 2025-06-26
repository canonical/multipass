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

#pragma once

#include <multipass/cli/client_common.h>
#include <multipass/cli/return_codes.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/terminal.h>
#include <multipass/timer.h>

#include <QString>

using RpcMethod = multipass::Rpc::StubInterface;

namespace multipass
{
class AnimatedSpinner;
class ArgParser;
class Formatter;
class SettingsException;

namespace cmd
{
const QString all_option_name{"all"};
const QString format_option_name{"format"};

ParseCode check_for_name_and_all_option_conflict(const ArgParser* parser,
                                                 std::ostream& cerr,
                                                 bool allow_empty = false);
InstanceNames add_instance_names(const ArgParser* parser);
InstanceNames add_instance_names(const ArgParser* parser, const std::string& default_name);
std::vector<InstanceSnapshotPair> add_instance_and_snapshot_names(const ArgParser* parser);
ParseCode handle_format_option(const ArgParser* parser,
                               Formatter** chosen_formatter,
                               std::ostream& cerr);
std::string instance_action_message_for(const InstanceNames& instance_names,
                                        const std::string& action_name);
ReturnCode run_cmd(const QStringList& args,
                   const ArgParser* parser,
                   std::ostream& cout,
                   std::ostream& cerr);
ReturnCode run_cmd_and_retry(const QStringList& args,
                             const ArgParser* parser,
                             std::ostream& cout,
                             std::ostream& cerr);
ReturnCode return_code_from(const SettingsException& e);
QString describe_common_settings_keys();

// parser helpers
void add_timeout(multipass::ArgParser*);
int parse_timeout(const multipass::ArgParser* parser);
std::unique_ptr<multipass::utils::Timer> make_timer(int timeout,
                                                    AnimatedSpinner* spinner,
                                                    std::ostream& cerr,
                                                    const std::string& msg);

} // namespace cmd
} // namespace multipass
