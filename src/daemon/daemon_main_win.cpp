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
 */

#include "cli.h"
#include "daemon.h"
#include "daemon_config.h"
#include "daemon_monitor_settings.h" // temporary

#include <multipass/constants.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/utils.h>
#include <multipass/version.h>

#include <fmt/format.h>

#include <QCoreApplication>

#include <windows.h>

#include <array>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto service_name = "Multipass";

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

class MessageBuf
{
public:
    ~MessageBuf()
    {
        if (buf)
            LocalFree(buf);
    }
    auto data()
    {
        return buf;
    }
    auto get()
    {
        return &buf;
    }

private:
    char* buf = nullptr;
};

std::string last_error_message()
{
    auto error = GetLastError();
    if (error == ERROR_SUCCESS)
        return std::string();

    MessageBuf buf;
    auto size = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<char*>(buf.get()), 0, nullptr);

    std::string message{buf.data(), size};
    mp::utils::trim_end(message);
    return message;
}

class ServiceHandle
{
public:
    ServiceHandle(SC_HANDLE handle, const char* msg) : handle{handle}
    {
        if (handle == nullptr)
            throw std::runtime_error(fmt::format("{} failed: '{}'", msg, last_error_message()));
    }

    ~ServiceHandle()
    {
        if (handle)
            CloseServiceHandle(handle);
    }

    SC_HANDLE get() const
    {
        return handle;
    }

private:
    SC_HANDLE handle;
};

std::string get_command_to_start_service()
{
    std::array<char, MAX_PATH> path;

    DWORD size{static_cast<DWORD>(path.size())};
    auto ok = GetModuleFileName(nullptr, path.data(), size);
    if (!ok)
        throw std::runtime_error(fmt::format("GetModuleFileName failed: '{}'", last_error_message()));

    return fmt::format("\"{}\" /svc", path.data());
}

void install_service()
{
    ServiceHandle sc_manager{OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE),
                             "OpenSCManager"};

    const auto command_to_start_service = get_command_to_start_service();
    ServiceHandle service{CreateService(sc_manager.get(),
                                        service_name,                          // Service name
                                        "Multipass Service",                   // Service display name
                                        SERVICE_START | SERVICE_CHANGE_CONFIG, // Access type to the service
                                        SERVICE_WIN32_OWN_PROCESS,             // Type of service
                                        SERVICE_AUTO_START,                    // How the service should start
                                        SERVICE_ERROR_NORMAL,                  // Error handling type
                                        command_to_start_service.c_str(),      // Path to the service binary
                                        nullptr,                               // load ordering group
                                        nullptr,                               // tag identifier
                                        nullptr,                               // Service dependencies
                                        "LocalSystem",                         // Account which will run the service
                                        nullptr                                // The password for the account above
                                        ),
                          "CreateService"};

    SERVICE_DESCRIPTION description{"Provides a service to create and manage virtual machines"};
    auto changed = ChangeServiceConfig2(service.get(), SERVICE_CONFIG_DESCRIPTION, &description);
    if (!changed)
        throw std::runtime_error(fmt::format("ChangeServiceConfig2 failed: '{}'", last_error_message()));

    SERVICE_FAILURE_ACTIONS sfa;
    SC_ACTION action;

    action.Type = SC_ACTION_RESTART;
    action.Delay = 2000;

    sfa.dwResetPeriod = INFINITE;
    sfa.lpCommand = nullptr;
    sfa.lpRebootMsg = nullptr;
    sfa.cActions = 1;
    sfa.lpsaActions = &action;

    changed = ChangeServiceConfig2(service.get(), SERVICE_CONFIG_FAILURE_ACTIONS, &sfa);
    if (!changed)
        throw std::runtime_error(fmt::format("ChangeServiceConfig2 failed: '{}'", last_error_message()));

    auto started = StartService(service.get(), 0, nullptr);
    if (!started)
        throw std::runtime_error(fmt::format("StartService failed: '{}'", last_error_message()));
}

void uninstall_service()
{
    ServiceHandle sc_manager{OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT), "OpenSCManager"};
    ServiceHandle service{OpenService(sc_manager.get(), service_name, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE),
                          "OpenService"};

    SERVICE_STATUS status;
    auto success = ControlService(service.get(), SERVICE_CONTROL_STOP, &status);
    if (success)
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (QueryServiceStatus(service.get(), &status) && std::chrono::steady_clock::now() < deadline)
        {
            if (status.dwCurrentState != SERVICE_STOP_PENDING)
                break;

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    if (!DeleteService(service.get()))
        throw std::runtime_error(fmt::format("DeleteService failed: '{}'", last_error_message()));
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

int daemon_main(int argc, char* argv[], RegisterConsoleHandler register_console)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(mp::daemon_name);
    QCoreApplication::setApplicationVersion(mp::version_string);

    if (register_console == RegisterConsoleHandler::yes)
        SetConsoleCtrlHandler(windows_console_ctrl_handler, TRUE);

    auto builder = mp::cli::parse(app);
    auto config = builder.build();

    mp::monitor_and_quit_on_settings_change(); // temporary
    mp::Daemon daemon(std::move(config));

    auto ret = QCoreApplication::exec();
    mpl::log(mpl::Level::info, "daemon", "Goodbye!");
    return ret;
}

void service_main(DWORD argc, char* argv[]) // clang-format off
try // clang-format on
{
    auto logger = mp::platform::make_logger(mpl::Level::info);
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

    auto exit_code = daemon_main(static_cast<int>(argc), argv, RegisterConsoleHandler::no);

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
    if (argc > 1)
    {
        std::string cmd{argv[1]};
        if (cmd == "/install")
        {
            install_service();
            return EXIT_SUCCESS;
        }

        if (cmd == "/uninstall")
        {
            uninstall_service();
            return EXIT_SUCCESS;
        }

        if (cmd == "/svc")
        {
            auto logger = mp::platform::make_logger(mpl::Level::info);
            logger->log(mpl::Level::info, "main", "calling service ctrl dispatcher");
            std::array<SERVICE_TABLE_ENTRY, 2> table{{{"", service_main}, {nullptr, nullptr}}};
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
