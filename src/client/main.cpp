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

#include <multipass/cli/cli.h>
#include <multipass/platform.h>

#include <QCoreApplication>

namespace mp = multipass;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("ubuntu");

    mp::ClientConfig config{mp::Platform::default_server_address(), std::cout, std::cerr};
    mp::Client client{config};

    return client.run(app.arguments());
}
