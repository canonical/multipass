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
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::ClientGui::ClientGui(ClientConfig& config)
    : cert_provider{std::move(config.cert_provider)},
      rpc_channel{mp::client::make_channel(config.server_address, config.conn_type, *cert_provider)},
      stub{mp::Rpc::NewStub(rpc_channel)},
      cout{config.cout},
      cerr{config.cerr},
      gui_cmd{std::make_unique<cmd::GuiCmd>(*rpc_channel, *stub, cout, cerr)}
{
}

int mp::ClientGui::run(const QStringList& arguments)
{
    QString description("Create, control and connect to Ubuntu instances.\n\n"
                        "This is a command line utility for multipass, a\n"
                        "service that manages Ubuntu instances.");

    ArgParser parser;
    parser.setApplicationDescription(description);

    // ParseCode parse_status = parser.parse();
    // if (parse_status != ParseCode::Ok)
    //{
    //    return parser.returnCodeFrom(parse_status);
    //}

    auto logger = std::make_shared<mpl::StandardLogger>(mpl::level_from(parser.verbosityLevel()));
    mpl::set_logger(logger);

    return gui_cmd->run(&parser);
}
