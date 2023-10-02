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

#include "clone.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Clone::run(ArgParser* parser)
{

    return {};
}

std::string cmd::Clone::name() const
{
    return "clone";
}

QString cmd::Clone::short_help() const
{
    return QStringLiteral("Clone a Ubuntu instance");
}

QString cmd::Clone::description() const
{
    return QStringLiteral("A clone is a complete independent copy of a whole virtual machine instance");
}

mp::ParseCode cmd::Clone::parse_args(ArgParser* parser)
{

    return ParseCode::Ok;
}
