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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "daemon.h"
#include "daemon_config.h"

#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <QCoreApplication>

#include <windows.h>

namespace
{
BOOL WINAPI windows_console_ctrl_handler(_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        {
            auto app = QCoreApplication::instance();
            app->quit();
            return TRUE;
        }
        default:
            return FALSE;
    }
}
}

int main(int argc, char* argv[])
try
{
    QCoreApplication app(argc, argv);
    SetConsoleCtrlHandler(windows_console_ctrl_handler, TRUE);

    auto config = multipass::DaemonConfigBuilder{}.build();

    multipass::logging::set_logger(config->logger);
    multipass::Daemon daemon(std::move(config));

    app.exec();

    fmt::print(stderr, "Goodbye!\n");
    return EXIT_SUCCESS;
}
catch (const std::exception& e)
{
    fmt::print(stderr, "Error: {}\n", e.what());
}
