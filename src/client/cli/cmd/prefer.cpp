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

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/argparser.h>

#include "common_cli.h"
#include "prefer.h"

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Prefer::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    aliases.set_active_context(parser->positionalArguments()[0].toStdString());

    return ReturnCode::Ok;
}

std::string cmd::Prefer::name() const
{
    return "prefer";
}

QString cmd::Prefer::short_help() const
{
    return QStringLiteral("Switch the current alias context");
}

QString cmd::Prefer::description() const
{
    return QStringLiteral("Switch the current alias context. If it does not exist, create it before switching.");
}

mp::ParseCode cmd::Prefer::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Name of the context to switch to", "<name>");

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() == 0)
    {
        cerr << "The prefer command needs an argument\n";
        return ParseCode::CommandLineError;
    }

    if (parser->positionalArguments().count() > 1)
    {
        cerr << "Wrong number of arguments given\n";
        return ParseCode::CommandLineError;
    }

    return status;
}
