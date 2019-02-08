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

#include <multipass/cli/client_common.h>

#include <QApplication>

namespace mp = multipass;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("multipass-gui");

    mp::ClientConfig config{mp::client::get_server_address(), mp::RpcConnectionType::ssl,
                            std::move(mp::client::get_cert_provider()), std::cout, std::cerr};
    mp::ClientGui client{config};

    return client.run(app.arguments());
}
