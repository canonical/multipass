/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include "client.h"
#include "cmd/alias.h"
#include "cmd/aliases.h"
#include "cmd/authenticate.h"
#include "cmd/delete.h"
#include "cmd/exec.h"
#include "cmd/find.h"
#include "cmd/get.h"
#include "cmd/help.h"
#include "cmd/info.h"
#include "cmd/launch.h"
#include "cmd/list.h"
#include "cmd/mount.h"
#include "cmd/networks.h"
#include "cmd/purge.h"
#include "cmd/recover.h"
#include "cmd/remote_settings_handler.h"
#include "cmd/restart.h"
#include "cmd/set.h"
#include "cmd/shell.h"
#include "cmd/start.h"
#include "cmd/stop.h"
#include "cmd/suspend.h"
#include "cmd/transfer.h"
#include "cmd/umount.h"
#include "cmd/unalias.h"
#include "cmd/version.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/settings/settings.h>
#include <multipass/top_catch_all.h>

#include <scope_guard.hpp>

#include <algorithm>
#include <memory>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
auto make_handler_unregisterer(mp::SettingsHandler* handler)
{
    return sg::make_scope_guard([handler]() noexcept {
        mp::top_catch_all("client", [handler] {
            MP_SETTINGS.unregister_handler(handler); // trust me clang-format
        });
    });
}
} // namespace

mp::Client::Client(ClientConfig& config)
    : stub{mp::Rpc::NewStub(mp::client::make_channel(config.server_address, config.cert_provider.get()))},
      term{config.term},
      aliases{config.term}
{
    add_command<cmd::Alias>(aliases);
    add_command<cmd::Aliases>(aliases);
    add_command<cmd::Authenticate>();
    add_command<cmd::Launch>(config.url_downloader);
    add_command<cmd::Purge>(aliases);
    add_command<cmd::Exec>(aliases);
    add_command<cmd::Find>();
    add_command<cmd::Get>();
    add_command<cmd::Help>();
    add_command<cmd::Info>();
    add_command<cmd::List>();
    add_command<cmd::Networks>();
    add_command<cmd::Mount>();
    add_command<cmd::Recover>();
    add_command<cmd::Set>();
    add_command<cmd::Shell>();
    add_command<cmd::Start>();
    add_command<cmd::Stop>();
    add_command<cmd::Suspend>();
    add_command<cmd::Transfer>();
    add_command<cmd::Unalias>(aliases);
    add_command<cmd::Restart>();
    add_command<cmd::Delete>(aliases);
    add_command<cmd::Umount>();
    add_command<cmd::Version>();

    sort_commands();
}

void mp::Client::sort_commands()
{
    auto name_sort = [](cmd::Command::UPtr& a, cmd::Command::UPtr& b) { return a->name() < b->name(); };
    std::sort(commands.begin(), commands.end(), name_sort);
}

int mp::Client::run(const QStringList& arguments)
{
    QString description("Create, control and connect to Ubuntu instances.\n\n"
                        "This is a command line utility for multipass, a\n"
                        "service that manages Ubuntu instances.");

    ArgParser parser(arguments, commands, term->cout(), term->cerr());
    parser.setApplicationDescription(description);

    mp::ReturnCode ret = mp::ReturnCode::Ok;
    ParseCode parse_status = parser.parse(aliases);

    auto verbosity = parser.verbosityLevel(); // try to respect requested verbosity, even if parsing failed
    if (!mpl::get_logger())
        mp::client::set_logger(mpl::level_from(verbosity));

    {
        auto daemon_settings_prefix = QString{daemon_settings_root} + ".";
        auto* handler = MP_SETTINGS.register_handler(
            std::make_unique<RemoteSettingsHandler>(std::move(daemon_settings_prefix), *stub, term, verbosity));
        auto handler_unregisterer = make_handler_unregisterer(handler); // remove handler before its dependencies expire

        try
        {
            mp::client::pre_setup();

            ret = parse_status == ParseCode::Ok ? parser.chosenCommand()->run(&parser)
                                                : parser.returnCodeFrom(parse_status);
        }
        catch (const RemoteHandlerException& e)
        {
            ret = mp::cmd::standard_failure_handler_for(parser.chosenCommand()->name(), term->cerr(), e.get_status());
        }
    }

    mp::client::post_setup();

    return ret;
}
