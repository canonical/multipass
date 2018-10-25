/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#include "restart.h"
#include "stop.h"
#include "start.h"

#include "animated_spinner.h"

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Restart::run(mp::ArgParser* parser)
{
  return ReturnCode::CommandFail; // TODO
}

std::string cmd::Restart::name() const { return "restart"; }

QString cmd::Restart::short_help() const
{
    return QStringLiteral("Restart instances");
}

QString cmd::Restart::description() const
{
    return QStringLiteral("Restart the named instances. Exits with return\n"
                          "code 0 when the instances restart, or with an\n"
                          "error code if any fail to restart.");
}

mp::ParseCode cmd::Restart::parse_args(mp::ArgParser* parser)
{ // TODO try to get rid of this
    return ParseCode::CommandFail;
}
