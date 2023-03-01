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

#include "snapshot.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = mp::cmd;

mp::ReturnCode cmd::Snapshot::run(mp::ArgParser* parser)
{
    return parser->returnCodeFrom(parse_args(parser)); // TODO@ricab implement
}

std::string cmd::Snapshot::name() const
{
    return "snapshot";
}

QString cmd::Snapshot::short_help() const
{
    return QStringLiteral("Take a snapshot of an instance");
}

QString cmd::Snapshot::description() const
{
    return QStringLiteral("Take a snapshot of an instance that can later be restored to recover the current state.");
}

mp::ParseCode cmd::Snapshot::parse_args(mp::ArgParser* parser)
{
    return parser->commandParse(this); // TODO@ricab implement
}
