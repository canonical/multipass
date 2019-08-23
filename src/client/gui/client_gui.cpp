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

#include "client_gui.h"
#include "argparser.h"

#include <multipass/cli/client_common.h>
#include <multipass/constants.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>
#include <multipass/settings.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::ClientGui::ClientGui(ClientConfig& config)
    : cert_provider{std::move(config.cert_provider)},
      rpc_channel{mp::client::make_channel(config.server_address, config.conn_type, *cert_provider)},
      stub{mp::Rpc::NewStub(rpc_channel)},
      gui_cmd{std::make_unique<cmd::GuiCmd>(*rpc_channel, *stub, null_stream, null_stream)}
{
}

int mp::ClientGui::run(const QStringList& arguments)
{
    auto ret = 0;
    ArgParser parser;

    QCommandLineOption autostart{"exit-unless-autostart",
                                 "Exit right away, not actually creating a GUI, unless configured to autostart. "
                                 "Pass this option when auto-starting to honor the autostart setting."};
    autostart.setFlags(QCommandLineOption::HiddenFromHelp);

    parser.addOption(autostart);
    parser.addHelpOption();
    parser.process(arguments);

    if (!parser.isSet(autostart) || Settings::instance().get_as<bool>(autostart_key))
    {
        auto logger = std::make_shared<mpl::StandardLogger>(mpl::level_from(parser.verbosityLevel()));
        mpl::set_logger(logger);

        ret = gui_cmd->run(&parser);
    }

    return ret;
}
