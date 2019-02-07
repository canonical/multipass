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

#include <multipass/platform.h>
#include <multipass/ssl_cert_provider.h>
#include <multipass/utils.h>

#include <QApplication>
#include <QStandardPaths>
#include <QtGlobal>

namespace mp = multipass;

namespace
{
std::string get_server_address()
{
    const auto address = qgetenv("MULTIPASS_SERVER_ADDRESS").toStdString();
    if (!address.empty())
    {
        mp::utils::validate_server_address(address);
        return address;
    }

    return mp::platform::default_server_address();
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("multipass-gui");

    auto data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    auto client_cert_dir = mp::utils::make_dir(data_dir, "client-certificate");
    auto cert_provider = std::make_unique<mp::SSLCertProvider>(client_cert_dir);

    mp::ClientConfig config{get_server_address(), mp::RpcConnectionType::ssl, std::move(cert_provider), std::cout,
                            std::cerr};
    mp::ClientGui client{config};

    return client.run(QCoreApplication::arguments());
}
