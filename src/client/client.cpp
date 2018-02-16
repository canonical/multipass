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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */
#include "client.h"
#include "cmd/create.h"
#include "cmd/empty_trash.h"
#include "cmd/exec.h"
#include "cmd/find.h"
#include "cmd/help.h"
#include "cmd/info.h"
#include "cmd/list.h"
#include "cmd/mount.h"
#include "cmd/recover.h"
#include "cmd/shell.h"
#include "cmd/start.h"
#include "cmd/stop.h"
#include "cmd/trash.h"
#include "cmd/umount.h"
#include "cmd/version.h"

#include <grpc++/grpc++.h>

#include <algorithm>

#include <multipass/cli/argparser.h>
#include <multipass/cli/json_formatter.h>
#include <multipass/cli/table_formatter.h>

namespace mp = multipass;

namespace
{
template <typename T>
std::unique_ptr<mp::Formatter> make_entry()
{
    return std::make_unique<T>();
}

auto make_map()
{
    std::map<std::string, std::unique_ptr<mp::Formatter>> map;
    map.emplace("table", make_entry<mp::TableFormatter>());
    map.emplace("json", make_entry<mp::JsonFormatter>());

    return map;
}
}

mp::Client::Client(const ClientConfig& config)
    : rpc_channel{grpc::CreateChannel(config.server_address, grpc::InsecureChannelCredentials())},
      stub{mp::Rpc::NewStub(rpc_channel)},
      formatters{make_map()},
      cout{config.cout},
      cerr{config.cerr}
{
    add_command<cmd::Create>();
    add_command<cmd::EmptyTrash>();
    add_command<cmd::Exec>();
    add_command<cmd::Find>();
    add_command<cmd::Help>();
    add_command<cmd::Info>();
    add_command<cmd::List>();
    add_command<cmd::Mount>();
    add_command<cmd::Recover>();
    add_command<cmd::Shell>();
    add_command<cmd::Start>();
    add_command<cmd::Stop>();
    add_command<cmd::Trash>();
    add_command<cmd::Umount>();
    add_command<cmd::Version>();

    auto name_sort = [](cmd::Command::UPtr& a, cmd::Command::UPtr& b) { return a->name() < b->name(); };
    std::sort(commands.begin(), commands.end(), name_sort);
}

template <typename T>
void mp::Client::add_command()
{
    auto cmd = std::make_unique<T>(*rpc_channel, *stub, &formatters, cout, cerr);
    commands.push_back(std::move(cmd));
}

int mp::Client::run(const QStringList& arguments)
{
    QString description("Create, control and connect to Ubuntu instances.\n\n"
                        "This is a command line utility for multipass, a\n"
                        "service that manages Ubuntu instances.");

    ArgParser parser(arguments, commands, cout, cerr);
    parser.setApplicationDescription(description);

    ParseCode parse_status = parser.parse();
    if (parse_status != ParseCode::Ok)
    {
        return parser.returnCodeFrom(parse_status);
    }
    return parser.chosenCommand()->run(&parser);
}
