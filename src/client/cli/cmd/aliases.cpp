/*
 * Copyright (C) 2021 Canonical, Ltd.
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
#include <multipass/cli/formatter.h>

#include "aliases.h"
#include "common_cli.h"

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Aliases::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    cout << chosen_formatter->format(aliases);

    return ReturnCode::Ok;
}

std::string cmd::Aliases::name() const
{
    return "aliases";
}

QString cmd::Aliases::short_help() const
{
    return QStringLiteral("List available aliases");
}

QString cmd::Aliases::description() const
{
    return QStringLiteral("List available aliases");
}

mp::ParseCode cmd::Aliases::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption(
        "format", "Output list in the requested format.\nValid formats are: table (default), json, csv and yaml",
        "format", "table");

    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments\n";
        return ParseCode::CommandLineError;
    }

    status = handle_format_option(parser, &chosen_formatter, cerr);

    return status;
}
