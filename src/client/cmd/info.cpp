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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *
 */

#include "info.h"

#include <multipass/cli/argparser.h>

#include <iomanip>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

namespace
{
std::ostream& operator<<(std::ostream& out, const multipass::InfoReply_Status& status)
{
    switch(status)
    {
    case mp::InfoReply::RUNNING:
        out << "RUNNING";
        break;
    case mp::InfoReply::STOPPED:
        out << "STOPPED";
        break;
    case mp::InfoReply::TRASHED:
        out << "IN TRASH";
        break;
    default:
        out << "UNKOWN";
        break;
    }
    return out;
}
std::ostream& operator<<(std::ostream& out, const multipass::InfoReply::Info& info)
{
    std::string ipv4{info.ipv4()};
    std::string ipv6{info.ipv6()};
    if (ipv4.empty())
        ipv4.append("--");
    if (ipv6.empty())
        ipv6.append("--");
    out << std::setw(16) << std::left << "Name:";
    out << info.name() << "\n";

    out << std::setw(16) << std::left << "State:";
    out << info.status() << "\n";

    out << std::setw(16) << std::left << "IPv4:";
    out << ipv4 << "\n";

    out << std::setw(16) << std::left << "IPV6:";
    out << ipv6 << "\n";

    out << std::setw(16) << std::left << "Release:";
    out << "Ubuntu " << info.release() << "\n";

    out << std::setw(16) << std::left << "Image hash:";
    out << info.id().substr(0, 12) << "\n";

    out << std::setw(16) << std::left << "Load:";
    out << info.load() << "\n";

    out << std::setw(16) << std::left << "Disk usage:";
    out << info.disk_usage() << "%\n";

    out << std::setw(16) << std::left << "Memory usage:";
    out << info.memory_usage() << "%\n";

    auto mount_paths = info.mount_info().mount_paths();
    for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
    {
        out << std::setw(16) << std::left << ((mount == mount_paths.cbegin()) ? "Mounts:" : " ")
            << std::setw(info.mount_info().longest_path_len()) << mount->source_path() << "  => "
            << mount->target_path() << "\n";
    }
    return out;
}
}

mp::ReturnCode cmd::Info::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::InfoReply& reply) {
        std::stringstream out;
        for (const auto& info : reply.info())
        {
            out << info;
            out << "\n";
        }
        cout << out.str();
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "info failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::info, request, on_success, on_failure);
}

std::string cmd::Info::name() const { return "info"; }

QString cmd::Info::short_help() const
{
    return QStringLiteral("Display information about instances");
}

QString cmd::Info::description() const
{
    return QStringLiteral("Display information about instances");
}

mp::ParseCode cmd::Info::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to display information about", "<name> [<name> ...]");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() == 0)
    {
        cerr << "Name argument is required" << std::endl;
        status = ParseCode::CommandLineError;
    }
    else
    {
        for (const auto& arg : parser->positionalArguments())
        {
            auto entry = request.add_instance_name();
            entry->append(arg.toStdString());
        }
    }

    return status;
}
