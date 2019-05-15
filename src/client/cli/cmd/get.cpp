/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "get.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>

#include <QSettings>
#include <QtGlobal>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Get::run(mp::ArgParser* parser)
{
    auto parse_code = parse_args(parser);
    auto ret = parser->returnCodeFrom(parse_code);
    if (parse_code == ParseCode::Ok)
    {
        QSettings settings;

        const auto val = settings.value(key);
        if (val.isNull())
        {
            cerr << "Unknown key: \"" << qPrintable(key) << "\"\n";
            ret = ReturnCode::CommandLineError;
        }
        else
        {
            cout << qPrintable(val.toString()) << "\n";
        }
    }

    return ret;
}

std::string cmd::Get::name() const
{
    return "get";
}

QString cmd::Get::short_help() const
{
    return QStringLiteral("Get a configuration option");
}

QString cmd::Get::description() const
{
    return QStringLiteral("Get the configuration option corresponding to the given key");
}

mp::ParseCode cmd::Get::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("key", "Path to the option whose configured value should be obtained.", "<key>");

    auto status = parser->commandParse(this);
    if (status == ParseCode::Ok)
    {
        const auto args = parser->positionalArguments();
        if (args.count() == 1)
        {
            key = args.at(0);
        }
        else
        {
            cerr << "Need exactly one option key.\n";
            status = ParseCode::CommandLineError;
        }
    }

    return status;
}
