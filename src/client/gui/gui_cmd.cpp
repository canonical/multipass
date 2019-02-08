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

#include "gui_cmd.h"
#include "argparser.h"

#include <multipass/cli/client_common.h>
#include <multipass/cli/client_platform.h>
#include <multipass/cli/format_utils.h>

#include <QEventLoop>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

mp::ReturnCode cmd::GuiCmd::run(mp::ArgParser* parser)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        cerr << "System tray not supported\n";
        return ReturnCode::CommandFail;
    }

    QEventLoop loop;

    create_actions();
    create_menu();
    tray_icon.show();

    return static_cast<ReturnCode>(loop.exec());
}

std::string cmd::GuiCmd::name() const
{
    return "";
}

QString cmd::GuiCmd::short_help() const
{
    return QStringLiteral("");
}

QString cmd::GuiCmd::description() const
{
    return QStringLiteral("");
}

mp::ParseCode cmd::GuiCmd::parse_args(mp::ArgParser* parser)
{
    return ParseCode::Ok;
}

void cmd::GuiCmd::create_actions()
{
    retrieving_action = tray_icon_menu.addAction("Retrieving instances...");
    about_separator = tray_icon_menu.addSeparator();
    about_action = tray_icon_menu.addAction("About");
    quit_action = tray_icon_menu.addAction("Quit");
}

void cmd::GuiCmd::update_menu()
{
    tray_icon_menu.removeAction(retrieving_action);

    auto reply = list_future.result();
    instances_menus.clear();

    for (const auto& instance : reply.instances())
    {
        auto state = QString::fromStdString(mp::format::status_string_for(instance.instance_status()));

        if (state != "DELETED")
        {
            auto name = instance.name();
            auto action_string = QString("%1 (%2)").arg(QString::fromStdString(name)).arg(state);

            instances_menus[name].instance_menu = std::make_unique<QMenu>(action_string);
            auto& instance_menu = instances_menus.at(name).instance_menu;
            auto& instance_actions = instances_menus.at(name).instance_actions;

            instance_actions.push_back(instance_menu->addAction("Open shell"));
            QObject::connect(instance_actions.back(), &QAction::triggered, [name] {
                fmt::print("Opening shell for {}\n", name);
                mp::cli::platform::open_multipass_shell(QString::fromStdString(name));
            });

            if (state != "RUNNING")
            {
                instance_actions.back()->setDisabled(true);
            }

            if (state == "RUNNING")
            {
                instance_actions.push_back(instance_menu->addAction("Suspend"));
                QObject::connect(instance_actions.back(), &QAction::triggered, [this, name] {
                    fmt::print("Suspending {}\n", name);
                    future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::suspend_instance_for, name));
                });

                instance_actions.push_back(instance_menu->addAction("Stop"));
                QObject::connect(instance_actions.back(), &QAction::triggered, [this, name] {
                    fmt::print("Stopping {}\n", name);
                    future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::stop_instance_for, name));
                });
            }
            else if (state == "STOPPED" || state == "SUSPENDED")
            {
                instance_actions.push_back(instance_menu->addAction("Start"));
                QObject::connect(instance_actions.back(), &QAction::triggered, [this, name] {
                    fmt::print("Started {}\n", name);
                    future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::start_instance_for, name));
                });
            }

            tray_icon_menu.insertMenu(about_separator, instance_menu.get());
        }
    }

    if (instances_menus.empty())
    {
        about_separator->setVisible(false);
    }
    else
    {
        about_separator->setVisible(true);
    }
}

void cmd::GuiCmd::create_menu()
{
    tray_icon.setContextMenu(&tray_icon_menu);

    tray_icon.setIcon(QIcon{":images/ubuntu-icon.png"});

    QObject::connect(&list_watcher, &QFutureWatcher<ListReply>::finished, this, &GuiCmd::update_menu);

    QObject::connect(&tray_icon_menu, &QMenu::aboutToShow, [this]() {
        if (failure_action.isVisible())
        {
            tray_icon_menu.removeAction(&failure_action);
        }

        if (instances_menus.empty())
            tray_icon_menu.insertAction(about_separator, retrieving_action);

        if (!list_future.isRunning())
        {
            list_future = QtConcurrent::run(this, &GuiCmd::retrieve_all_instances);
            future_synchronizer.addFuture(list_future);
            list_watcher.setFuture(list_future);
        }
    });

    // Use a singleShot here to make sure the event loop is running before the quit() runs
    QObject::connect(quit_action, &QAction::triggered, [this] {
        future_synchronizer.waitForFinished();
        QTimer::singleShot(0, [] { QCoreApplication::quit(); });
    });
}

mp::ListReply cmd::GuiCmd::retrieve_all_instances()
{
    ListReply list_reply;
    auto on_success = [&list_reply](ListReply& reply) {
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

void cmd::GuiCmd::start_instance_for(const std::string& instance_name)
{
    auto on_success = [](mp::StartReply& reply) { return ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    StartRequest request;
    auto names = request.mutable_instance_names()->add_instance_name();
    names->append(instance_name);

    dispatch(&RpcMethod::start, request, on_success, on_failure);
}

void cmd::GuiCmd::stop_instance_for(const std::string& instance_name)
{
    auto on_success = [](mp::StopReply& reply) { return ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    StopRequest request;
    auto names = request.mutable_instance_names()->add_instance_name();
    names->append(instance_name);

    dispatch(&RpcMethod::stop, request, on_success, on_failure);
}

void cmd::GuiCmd::suspend_instance_for(const std::string& instance_name)
{
    auto on_success = [](mp::SuspendReply& reply) { return ReturnCode::Ok; };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    SuspendRequest request;
    auto names = request.mutable_instance_names()->add_instance_name();
    names->append(instance_name);

    dispatch(&RpcMethod::suspend, request, on_success, on_failure);
}
