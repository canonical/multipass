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
 */

#include "find.h"

#include <multipass/cli/argparser.h>

#include <iomanip>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

namespace
{
std::ostream& operator<<(std::ostream& out, const multipass::FindReply::ImageInfo& image_info)
{
    bool first = false;

    for (const auto& alias : image_info.aliases())
    {
        out << std::setw(21) << std::left << alias << (!first ? "Ubuntu " + image_info.release() : "\"") << "\n";
        first = true;
    }

    return out;
}
}

mp::ReturnCode cmd::Find::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::FindReply& reply) {
        std::stringstream out;

        out << "multipass launch …   Creates an instance of\n";
        out << "-------------------------------------------\n";

        for (const auto& info : reply.images_info())
        {
            out << info;
        }
        cout << out.str();

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "find failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::find, request, on_success, on_failure);
}

std::string cmd::Find::name() const
{
    return "find";
}

QString cmd::Find::short_help() const
{
    return QStringLiteral("Display available images to create instances from");
}

QString cmd::Find::description() const
{
    return QStringLiteral("Lists available images including <string> for creating instances from.\n"
                          "With no search string, lists all aliases for supported Ubuntu releases.");
}

mp::ParseCode cmd::Find::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("string", "Optionally find images matching this string", "[<remote:>]<string>");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Wrong number of arguments" << std::endl;
        status = ParseCode::CommandLineError;
    }
    else if (parser->positionalArguments().count() == 1)
    {
        auto search_string = parser->positionalArguments().first();
        auto colon_count = search_string.count(':');

        if (colon_count > 1)
        {
            cerr << "Invalid remote and search string supplied" << std::endl;
            return ParseCode::CommandLineError;
        }
        else if (colon_count == 1)
        {
            request.set_remote_name(search_string.section(':', 0, 0).toStdString());
            request.set_search_string(search_string.section(':', 1).toStdString());
        }
        else
        {
            request.set_search_string(search_string.toStdString());
        }
    }

    return status;
}
