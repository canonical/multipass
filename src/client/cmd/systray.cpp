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

#include "systray.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/format_utils.h>

#include <QEventLoop>
#include <QTimer>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::Systray::run(mp::ArgParser* parser)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        cerr << "System tray not supported\n";
        return ReturnCode::CommandFail;
    }

    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    QEventLoop loop;

    create_actions();
    create_menu();
    tray_icon.show();

    return static_cast<ReturnCode>(loop.exec());
}

std::string cmd::Systray::name() const
{
    return "systray";
}

QString cmd::Systray::short_help() const
{
    return QStringLiteral("Run client in system tray");
}

QString cmd::Systray::description() const
{
    return QStringLiteral("Run the client in the system tray.");
}

mp::ParseCode cmd::Systray::parse_args(mp::ArgParser* parser)
{
    return parser->commandParse(this);
}

void cmd::Systray::create_actions()
{
    retrieving_action = tray_icon_menu.addAction("Retrieving instances...");
    about_separator = tray_icon_menu.addSeparator();
    about_action = tray_icon_menu.addAction("About");
    quit_action = tray_icon_menu.addAction("Quit");
}

void cmd::Systray::create_menu()
{
    tray_icon.setContextMenu(&tray_icon_menu);

    tray_icon.setIcon(QIcon{"./ubuntu-icon.png"});

    QObject::connect(&tray_icon_menu, &QMenu::aboutToShow, [this]() {
        if (failure_action.isVisible())
        {
            tray_icon_menu.removeAction(&failure_action);
        }

        std::unique_lock<decltype(worker_mutex)> lock{worker_mutex};
        if (!worker_running)
        {
            worker_running = true;
            worker = std::thread([this] {
                for (const auto& instance_menu : instances_menus)
                {
                    tray_icon_menu.removeAction(instance_menu->menuAction());
                }
                instances_menus.clear();
                tray_icon_menu.insertAction(about_separator, retrieving_action);

                retrieve_all_instances();

                std::unique_lock<decltype(worker_mutex)> lock{worker_mutex};
                worker_running = false;
            });

            worker.detach();
        }
    });

    // Use a singleShot here to make sure the event loop is running before the quit() runs
    QObject::connect(quit_action, &QAction::triggered,
                     [this]() { QTimer::singleShot(0, [] { QCoreApplication::quit(); }); });
}

void cmd::Systray::retrieve_all_instances()
{
    auto on_success = [this](ListReply& reply) {
        tray_icon_menu.removeAction(retrieving_action);

        for (const auto& instance : reply.instances())
        {
            auto state = QString::fromStdString(mp::format::status_string_for(instance.instance_status()));
            auto action_string = QString("%1 (%2)").arg(QString::fromStdString(instance.name())).arg(state);

            instances_menus.push_back(std::make_unique<QMenu>(action_string));

            if (state != "DELETED")
            {
                auto shell_action = instances_menus.back()->addAction("Open shell");

                if (state != "RUNNING")
                {
                    shell_action->setDisabled(true);
                }

                if (state != "STOPPED")
                {
                    instances_actions.push_back(instances_menus.back()->addAction("Stop"));
                    QObject::connect(instances_actions.back(), &QAction::triggered,
                                     [this]() { fmt::print("We would have stopped here\n"); });
                }
                else
                {
                    instances_menus.back()->addAction("Start");
                }

                tray_icon_menu.insertMenu(about_separator, instances_menus.back().get());
            }
        }

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        tray_icon_menu.removeAction(retrieving_action);
        tray_icon_menu.insertAction(about_separator, &failure_action);

        return standard_failure_handler_for(name(), cerr, status);
    };

    ListRequest request;
    dispatch(&RpcMethod::list, request, on_success, on_failure);
}
