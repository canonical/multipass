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
#include <multipass/version.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QtConcurrent/QtConcurrent>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;
using namespace std::chrono_literals;

mp::ReturnCode cmd::GuiCmd::run(mp::ArgParser* parser)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        cerr << "System tray not supported\n";
        return ReturnCode::CommandFail;
    }

    create_actions();
    create_menu();
    tray_icon.show();

    return static_cast<ReturnCode>(QCoreApplication::exec());
}

void cmd::GuiCmd::create_actions()
{
    retrieving_action = tray_icon_menu.addAction("Retrieving instances...");
    about_separator = tray_icon_menu.addSeparator();
    quit_action = tray_icon_menu.addAction("Quit");
}

void cmd::GuiCmd::update_menu()
{
    std::vector<std::string> instances_to_remove;
    tray_icon_menu.removeAction(retrieving_action);

    auto reply = list_future.result();

    for (auto it = instances_entries.cbegin(); it != instances_entries.cend(); ++it)
    {
        auto instance = std::find_if(reply.instances().cbegin(), reply.instances().cend(),
                                     [it](const ListVMInstance& instance) { return it->first == instance.name(); });

        if (instance == reply.instances().cend())
        {
            instances_to_remove.push_back(it->first);
        }
    }

    for (const auto instance : instances_to_remove)
    {
        instances_entries.erase(instance);
    }

    for (const auto& instance : reply.instances())
    {
        auto name = instance.name();
        auto state = QString::fromStdString(mp::format::status_string_for(instance.instance_status()));

        auto it = instances_entries.find(name);

        if (it != instances_entries.end())
        {
            auto instance_state = it->second.state;
            if (state == "DELETED")
            {
                instances_entries.erase(name);
            }
            else if (instance_state != state)
            {
                auto& instance_menu = instances_entries.at(name).menu;

                instance_menu->setTitle(QString("%1 (%2)").arg(QString::fromStdString(name)).arg(state));
                instance_menu->clear();
                set_menu_actions_for(name, state);
                instances_entries[name].state = state;
            }

            continue;
        }

        if (state != "DELETED")
        {
            instances_entries[name].menu =
                std::make_unique<QMenu>(QString("%1 (%2)").arg(QString::fromStdString(name)).arg(state));
            set_menu_actions_for(name, state);
            instances_entries[name].state = state;
        }
    }

    if (instances_entries.empty())
    {
        about_separator->setVisible(false);
    }
    else
    {
        about_separator->setVisible(true);
    }
}

void cmd::GuiCmd::update_about_menu()
{
    auto reply = version_future.result();

    about_client_version.setText("multipass version: " + QString::fromStdString(multipass::version_string));
    about_daemon_version.setText("multipassd version: " + QString::fromStdString(reply.version()));

    if (update_available(reply.update_info()))
    {
        update_action.setWhatsThis(QString::fromStdString(reply.update_info().url()));
        tray_icon_menu.insertAction(about_menu.menuAction(), &update_action);
    }
    else
    {
        tray_icon_menu.removeAction(&update_action);
    }
}

void cmd::GuiCmd::create_menu()
{
    tray_icon.setContextMenu(&tray_icon_menu);

    tray_icon.setIcon(QIcon{":images/ubuntu-icon.png"});

    QObject::connect(&list_watcher, &QFutureWatcher<ListReply>::finished, this, &GuiCmd::update_menu);

    QObject::connect(&menu_update_timer, &QTimer::timeout, [this]() { initiate_menu_layout(); });

    // Use a singleShot here to make sure the event loop is running before the quit() runs
    QObject::connect(quit_action, &QAction::triggered, [this] {
        future_synchronizer.waitForFinished();
        QTimer::singleShot(0, [] { QCoreApplication::quit(); });
    });

    QObject::connect(&version_watcher, &QFutureWatcher<VersionReply>::finished, this, &GuiCmd::update_about_menu);
    QObject::connect(&about_update_timer, &QTimer::timeout, [this]() { initiate_about_menu_layout(); });
    QObject::connect(&update_action, &QAction::triggered,
                     [this](bool checked) { QDesktopServices::openUrl(QUrl(update_action.whatsThis())); });

    about_menu.setTitle("About");

    about_client_version.setEnabled(false);
    about_daemon_version.setEnabled(false);
    about_copyright.setText("Copyright © 2017-2019 Canonical Ltd.");
    about_copyright.setEnabled(false);

    about_menu.insertActions(0, {&about_client_version, &about_daemon_version, &about_copyright});

    tray_icon_menu.insertMenu(quit_action, &about_menu);

    initiate_menu_layout();
    initiate_about_menu_layout();

    menu_update_timer.start(1s);
    about_update_timer.start(12h);
}

void cmd::GuiCmd::initiate_menu_layout()
{
    if (failure_action.isVisible())
    {
        tray_icon_menu.removeAction(&failure_action);
    }

    if (instances_entries.empty())
        tray_icon_menu.insertAction(about_separator, retrieving_action);

    if (!list_future.isRunning())
    {
        list_future = QtConcurrent::run(this, &GuiCmd::retrieve_all_instances);
        future_synchronizer.addFuture(list_future);
        list_watcher.setFuture(list_future);
    }
}

void cmd::GuiCmd::initiate_about_menu_layout()
{
    if (!version_future.isRunning())
    {
        version_future = QtConcurrent::run(this, &GuiCmd::retrieve_version_and_update_info);
        future_synchronizer.addFuture(version_future);
        version_watcher.setFuture(version_future);
    }
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

void cmd::GuiCmd::set_menu_actions_for(const std::string& instance_name, const QString& state)
{
    auto& instance_menu = instances_entries.at(instance_name).menu;

    instance_menu->addAction("Open shell");
    QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
        mp::cli::platform::open_multipass_shell(QString::fromStdString(instance_name));
    });

    if (state == "RUNNING")
    {
        instance_menu->addAction("Suspend");
        QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
            future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::suspend_instance_for, instance_name));
        });

        instance_menu->addAction("Stop");
        QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
            future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::stop_instance_for, instance_name));
        });
    }

    tray_icon_menu.insertMenu(about_separator, instance_menu.get());
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

mp::VersionReply cmd::GuiCmd::retrieve_version_and_update_info()
{
    VersionReply version_reply;

    auto on_success = [&version_reply](VersionReply& reply) {
        version_reply = reply;
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    VersionRequest request;
    dispatch(&RpcMethod::version, request, on_success, on_failure);

    return version_reply;
}
