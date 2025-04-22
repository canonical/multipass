/*
 * Copyright (C) Canonical, Ltd.
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

#include "cli.h"
#include "daemon.h"
#include "daemon_config.h"
#include "daemon_init_settings.h"

#include <multipass/cli/client_common.h>
#include <multipass/client_cert_store.h>
#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/ssl_cert_provider.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/version.h>

#include <QCoreApplication>

#include <windows.h>

#include <array>
#include <sstream>
#include <string>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;

#define BUFF_SZ 32767

namespace
{
constexpr auto service_name = "Multipass";
std::vector<char*> service_argv;

BOOL windows_console_ctrl_handler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    {
        QCoreApplication::quit();
        return TRUE;
    }
    default:
        return FALSE;
    }
}

enum class RegisterConsoleHandler
{
    no,
    yes
};

auto make_status()
{
    SERVICE_STATUS status{};
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_START_PENDING;
    status.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode = NO_ERROR;
    status.dwServiceSpecificExitCode = 0;
    status.dwWaitHint = 0;
    return status;
}

SERVICE_STATUS_HANDLE service_handle{nullptr};
void control_handler(DWORD control)
{
    switch (control)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
    {
        if (service_handle != nullptr)
        {
            auto status = make_status();
            status.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(service_handle, &status);
        }
        QCoreApplication::quit();
    }
    }
}

void create_client_cert_if_necessary()
{
    TCHAR infoBuf[BUFF_SZ];
    GetSystemDirectory(infoBuf, BUFF_SZ);
    auto storage_path = MP_PLATFORM.multipass_storage_location();

    const QString multipassd_data_dir_path{
        storage_path.isEmpty() ? QString("%1\\config\\systemprofile\\AppData\\Roaming\\multipassd\\").arg(infoBuf)
                               : QString("%1\\data").arg(storage_path)};

    mp::ClientCertStore cert_store{multipassd_data_dir_path};

    if (cert_store.empty())
    {
        auto data_location{MP_STDPATHS.writableLocation(mp::StandardPaths::GenericDataLocation)};
        auto client_cert_dir_path{MP_UTILS.make_dir(data_location + mp::common_client_cert_dir)};
        mp::SSLCertProvider cert_provider{client_cert_dir_path};

        cert_store.add_cert(cert_provider.PEM_certificate());
    }
}

int daemon_main(int argc, char* argv[], RegisterConsoleHandler register_console)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(mp::daemon_name);
    QCoreApplication::setApplicationVersion(mp::version_string);

    if (register_console == RegisterConsoleHandler::yes)
        SetConsoleCtrlHandler(windows_console_ctrl_handler, TRUE);

    mp::daemon::register_global_settings_handlers();

    auto builder = mp::cli::parse(app);
    auto config = builder.build();

    mp::daemon::monitor_and_quit_on_settings_change();
    mp::Daemon daemon(std::move(config));
    QObject::connect(&app,
                     &QCoreApplication::aboutToQuit,
                     &daemon,
                     &mp::Daemon::shutdown_grpc_server,
                     Qt::DirectConnection);

    mpl::log(mpl::Level::info, "daemon", fmt::format("Daemon arguments: {}", app.arguments().join(" ")));
    auto ret = QCoreApplication::exec();
    mpl::log(mpl::Level::info, "daemon", "Goodbye!");
    return ret;
}

void service_main(DWORD argc, char* argv[]) // clang-format off
try // clang-format on
{
    auto logger = mp::platform::make_logger(mpl::Level::info);
    auto daemon_argv(service_argv);
    daemon_argv.insert(daemon_argv.end(), argv + 1, argv + argc);

    logger->log(mpl::Level::info, "service_main", "registering control handler");

    service_handle = RegisterServiceCtrlHandler(service_name, control_handler);
    if (service_handle == nullptr)
    {
        logger->log(mpl::Level::info, "service_main", "failed to register control handler");
        return;
    }

    auto status = make_status();
    SetServiceStatus(service_handle, &status);
    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(service_handle, &status);

    logger->log(mpl::Level::info, "service_main", "service is running");

    auto exit_code = daemon_main(daemon_argv.size(), daemon_argv.data(), RegisterConsoleHandler::no);

    status.dwCurrentState = SERVICE_STOPPED;
    status.dwWin32ExitCode = exit_code;
    SetServiceStatus(service_handle, &status);

    logger->log(mpl::Level::info, "service_main", "service stopped");
}
catch (...)
{
    if (service_handle != nullptr)
    {
        auto status = make_status();
        status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(service_handle, &status);
    }
}
} // namespace

int main(int argc, char* argv[]) // clang-format off
try // clang-format on
{
    service_argv.assign(argv, argv + argc);

    auto logger = mp::platform::make_logger(mpl::Level::info);
    std::ostringstream arguments;
    std::copy(service_argv.begin(), service_argv.end(), std::ostream_iterator<std::string>(arguments, " "));
    logger->log(mpl::Level::info, "main", fmt::format("Starting Multipass {}", mp::version_string));
    logger->log(mpl::Level::info, "main", fmt::format("Service arguments: {}", arguments.str()));

    if (argc > 1)
    {
        std::string cmd{argv[1]};
        if (cmd == "/install")
        {
            QCoreApplication app(argc, argv);
            QCoreApplication::setApplicationName(mp::daemon_name);
            QCoreApplication::setApplicationVersion(mp::version_string);
            create_client_cert_if_necessary();
            return EXIT_SUCCESS;
        }

        if (cmd == "/svc")
        {
            logger->log(mpl::Level::info, "main", "calling service ctrl dispatcher");
            std::array<SERVICE_TABLE_ENTRY, 2> table{{{const_cast<char*>(""), service_main}, {nullptr, nullptr}}};
            // remove "/svc" from the list of arguments
            service_argv.erase(service_argv.begin() + 1);
            return StartServiceCtrlDispatcher(table.data());
        }
    }
    return daemon_main(argc, argv, RegisterConsoleHandler::yes);
}
catch (const std::exception& e)
{
    fmt::print(stderr, "error: {}\n", e.what());
    return EXIT_FAILURE;
}
