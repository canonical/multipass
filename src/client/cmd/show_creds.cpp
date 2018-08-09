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

#include "show_creds.h"

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

#include "show_creds.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::ShowCreds::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
        return parser->returnCodeFrom(ret);

    return ReturnCode::Ok;
}

std::string cmd::ShowCreds::name() const
{
    return "show-creds";
}

QString cmd::ShowCreds::short_help() const
{
    return QStringLiteral("Show public client credentials");
}

QString cmd::ShowCreds::description() const
{
    return QStringLiteral("Show public client credentials which can be used to register\n"
                          "with a remote multipass instance.");
}

mp::ParseCode cmd::ShowCreds::parse_args(mp::ArgParser* parser)
{
    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    cout << cert_provider.PEM_certificate();

    return status;
}
