/*
 * Copyright (C) 2018 Canonical, Ltd.
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

namespace multipass
{
class ArgParser;
class Formatter;

namespace cmd
{
const QString all_option_name{"all"};
const QString format_option_name{"format"};

ParseCode handle_all_option(ArgParser* parser);
InstanceNames add_instance_names(ArgParser* parser);
ParseCode handle_format_option(ArgParser* parser, Formatter** chosen_formatter);
} // namespace cmd
} // namespace multipass

#endif // MULTIPASS_COMMON_CLI_H
