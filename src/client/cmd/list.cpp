/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "list.h"

#include <multipass/cli/argparser.h>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

namespace
{
std::ostream& operator<<(std::ostream& out, const multipass::ListVMInstance_Status& status)
{
    switch (status)
    {
    case mp::ListVMInstance::RUNNING:
        out << "RUNNING";
        break;
    case mp::ListVMInstance::STOPPED:
        out << "STOPPED";
        break;
    case mp::ListVMInstance::TRASHED:
        out << "DELETED";
        break;
    default:
        out << "UNKNOWN";
        break;
    }
    return out;
}
}

mp::ReturnCode cmd::List::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](ListReply& reply) {
        const auto size = reply.instances_size();
        if (size < 1)
        {
            cout << "No instances found."
                 << "\n";
        }
        else
        {
            std::stringstream out;

            out << std::setw(24) << std::left << "Name";
            out << std::setw(9) << std::left << "State";
            out << std::setw(17) << std::left << "IPv4";
            out << std::left << "Release";
            out << "\n";

            for (int i = 0; i < size; i++)
            {
                const auto& instance = reply.instances(i);
                std::string ipv4{instance.ipv4()};
                std::string ipv6{instance.ipv6()};
                if (ipv4.empty())
                    ipv4.append("--");
                if (ipv6.empty())
                    ipv6.append("--");
                out << std::setw(24) << std::left << instance.name();
                out << std::setw(9) << std::left << instance.status();
                out << std::setw(17) << std::left << ipv4;
                out << instance.current_release();
                out << "\n";
            }
            cout << out.str();
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "list failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    ListRequest request;
    return dispatch(&RpcMethod::list, request, on_success, on_failure);
}

std::string cmd::List::name() const
{
    return "list";
}

std::vector<std::string> cmd::List::aliases() const
{
    return {name(), "ls"};
}

QString cmd::List::short_help() const
{
    return QStringLiteral("List all available instances");
}

QString cmd::List::description() const
{
    return QStringLiteral("List all instances which have been created.");
}

mp::ParseCode cmd::List::parse_args(mp::ArgParser* parser)
{
    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments" << std::endl;
        return ParseCode::CommandLineError;
    }

    return status;
}
