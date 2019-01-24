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
#include <QtConcurrent/QtConcurrent>

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

void cmd::Systray::update_menu()
{
    tray_icon_menu.removeAction(retrieving_action);

    auto reply = future.result();
    for (const auto& instance : reply.instances())
    {
        auto name = instance.name();
        auto state = QString::fromStdString(mp::format::status_string_for(instance.instance_status()));
        auto action_string = QString("%1 (%2)").arg(QString::fromStdString(name)).arg(state);

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
                                 [this, name] { fmt::print("We would have stopped {}\n", name); });
            }
            else
            {
                instances_actions.push_back(instances_menus.back()->addAction("Start"));
                QObject::connect(instances_actions.back(), &QAction::triggered,
                                 [this, name] { fmt::print("We would have started {}\n", name); });
            }

            tray_icon_menu.insertMenu(about_separator, instances_menus.back().get());
        }
    }
}

void cmd::Systray::create_menu()
{
    tray_icon.setContextMenu(&tray_icon_menu);

    tray_icon.setIcon(QIcon{"./ubuntu-icon.png"});

    QObject::connect(&watcher, &QFutureWatcher<ListReply>::finished, this, &Systray::update_menu);

    QObject::connect(&tray_icon_menu, &QMenu::aboutToShow, [this]() {
        if (failure_action.isVisible())
        {
            tray_icon_menu.removeAction(&failure_action);
        }

        for (const auto& instance_menu : instances_menus)
        {
            tray_icon_menu.removeAction(instance_menu->menuAction());
        }
        instances_menus.clear();
        tray_icon_menu.insertAction(about_separator, retrieving_action);

        if (!future.isRunning())
        {
            future = QtConcurrent::run(this, &Systray::retrieve_all_instances);
            watcher.setFuture(future);
        }
    });

    // Use a singleShot here to make sure the event loop is running before the quit() runs
    QObject::connect(quit_action, &QAction::triggered, [this] {
        if (future.isRunning())
            future.waitForFinished();
        QTimer::singleShot(0, [] { QCoreApplication::quit(); });
    });
}

mp::ListReply cmd::Systray::retrieve_all_instances()
{
    ListReply list_reply;
    auto on_success = [this, &list_reply](ListReply& reply) {
        list_reply = reply;

        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        tray_icon_menu.removeAction(retrieving_action);
        tray_icon_menu.insertAction(about_separator, &failure_action);

        return standard_failure_handler_for(name(), cerr, status);
    };

    ListRequest request;
    dispatch(&RpcMethod::list, request, on_success, on_failure);

    return list_reply;
}

void cmd::Systray::stop_instance(const std::string& instance_name)
{
    auto on_success = [](mp::StopReply& reply) { return ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    StopRequest request;
    auto names = request.instance_names.add_instance_name();
    names->append(instance_name);

    dispatch(&RpcMethod::stop, request, on_success, on_failure);
}
