/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include "client_gui.h"
#include "argparser.h"

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/platform.h>
#include <multipass/settings.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::ClientGui::ClientGui(ClientConfig& config)
    : rpc_channel{config.conn_type == mp::RpcConnectionType::ssl
                      ? mp::client::make_secure_channel(config.server_address, config.cert_provider.get())
                      : mp::client::make_insecure_channel(config.server_address)},
      stub{mp::Rpc::NewStub(rpc_channel)},
      gui_cmd{std::make_unique<cmd::GuiCmd>(*rpc_channel, *stub, null_stream, null_stream)}
{
}

int mp::ClientGui::run(const QStringList& arguments)
{
    mp::client::set_logger();        // we need logging for...
    mp::client::pre_setup();         // ... something we want to do even if the command was wrong

    ArgParser parser;

    QCommandLineOption autostart{"autostarting",
                                 "Exit right away, not actually creating a GUI, unless configured to autostart. "
                                 "Pass this option when auto-starting to honor the autostart setting."};
    autostart.setFlags(QCommandLineOption::HiddenFromHelp);

    parser.addOption(autostart);
    parser.addHelpOption();
    parser.process(arguments);

    auto ret = ReturnCode::Ok;
    if (!parser.isSet(autostart) || MP_SETTINGS.get_as<bool>(autostart_key))
        ret = gui_cmd->run(&parser);

    return ret;
}
