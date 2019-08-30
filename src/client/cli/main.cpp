/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <multipass/cli/client_common.h>
#include <multipass/console.h>
#include <multipass/constants.h>
#include <multipass/platform.h>

#include <QCoreApplication>

namespace mp = multipass;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(mp::client_name);

    mp::Console::setup_environment();
    auto term = mp::Terminal::make_terminal();

    mp::platform::preliminary_gui_autostart_setup();

    mp::ClientConfig config{mp::client::get_server_address(), mp::RpcConnectionType::ssl,
                            mp::client::get_cert_provider(), term.get()};
    mp::Client client{config};

    return client.run(QCoreApplication::arguments());
}
