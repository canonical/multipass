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

#include "version.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/version.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Version::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    cout << "multipass  " << multipass::version_string << "\n";

    auto on_success = [this](mp::VersionReply& reply) {
        cout << "multipassd " << reply.version() << "\n";
        if (term->is_live() && update_available(reply.update_info()))
            cout << update_notice(reply.update_info());
        return ReturnCode::Ok;
    };

    auto on_failure = [](grpc::Status& status) { return ReturnCode::Ok; };

    mp::VersionRequest request;
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::version, request, on_success, on_failure);
}

std::string cmd::Version::name() const
{
    return "version";
}

QString cmd::Version::short_help() const
{
    return QStringLiteral("Show version details");
}

QString cmd::Version::description() const
{
    return QStringLiteral("Display version information about the multipass command\n"
                          "and daemon.");
}

mp::ParseCode cmd::Version::parse_args(mp::ArgParser* parser)
{
    return parser->commandParse(this);
}
