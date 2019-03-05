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

#include "common_cli.h"
#include "exec.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/format_utils.h>

#include <fmt/ostream.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
mp::ReturnCode return_code_for(const grpc::StatusCode& code)
{
    return code == grpc::StatusCode::UNAVAILABLE ? mp::ReturnCode::DaemonFail : mp::ReturnCode::CommandFail;
}
} // namespace

mp::ParseCode cmd::check_for_name_and_all_option_conflict(mp::ArgParser* parser, std::ostream& cerr)
{
    auto num_names = parser->positionalArguments().count();
    if (num_names == 0 && !parser->isSet(all_option_name))
    {
        fmt::print(cerr, "Name argument or --all is required\n");
        return ParseCode::CommandLineError;
    }

    if (num_names > 0 && parser->isSet(all_option_name))
    {
        fmt::print(cerr, "Cannot specify name{} when --all option set\n", num_names > 1 ? "s" : "");
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

mp::InstanceNames cmd::add_instance_names(mp::ArgParser* parser)
{
    InstanceNames instance_names;

    for (const auto& arg : parser->positionalArguments())
    {
        auto instance_name = instance_names.add_instance_name();
        instance_name->append(arg.toStdString());
    }

    return instance_names;
}

mp::ParseCode cmd::handle_format_option(mp::ArgParser* parser, mp::Formatter** chosen_formatter, std::ostream& cerr)
{
    *chosen_formatter = mp::format::formatter_for(parser->value(format_option_name).toStdString());

    if (*chosen_formatter == nullptr)
    {
        fmt::print(cerr, "Invalid format type given.\n");
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

std::string cmd::instance_action_message_for(const mp::InstanceNames& instance_names, const std::string& action_name)
{
    std::string message{action_name};

    if (instance_names.instance_name().empty())
        message.append("all instances");
    else if (instance_names.instance_name().size() > 1)
        message.append("requested instances");
    else
        message.append(instance_names.instance_name().Get(0));

    return message;
}

mp::ReturnCode cmd::standard_failure_handler_for(const std::string& command, std::ostream& cerr,
                                                 const grpc::Status& status, const std::string& error_details)
{
    fmt::print(cerr, "{} failed: {}\n{}", command, status.error_message(),
               !error_details.empty()
                   ? fmt::format("{}\n", error_details)
                   : !status.error_details().empty() ? fmt::format("{}\n", status.error_details()) : "");

    return return_code_for(status.error_code());
}
