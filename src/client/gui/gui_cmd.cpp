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
#include <multipass/format.h>
#include <multipass/settings.h>
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
    about_separator = tray_icon_menu.addSeparator();
    quit_action = tray_icon_menu.addAction("Quit");

    petenv_actions_separator = tray_icon_menu.insertSeparator(tray_icon_menu.actions().first());
    tray_icon_menu.insertActions(petenv_actions_separator,
                                 {&petenv_start_action, &petenv_shell_action, &petenv_stop_action});

    QObject::connect(&petenv_shell_action, &QAction::triggered,
                     [] { mp::cli::platform::open_multipass_shell(QString()); });
    QObject::connect(&petenv_stop_action, &QAction::triggered, [this] {
        future_synchronizer.addFuture(
            QtConcurrent::run(this, &GuiCmd::stop_instance_for, Settings::instance().get(petenv_key).toStdString()));
    });
    QObject::connect(&petenv_start_action, &QAction::triggered, [this] {
        future_synchronizer.addFuture(
            QtConcurrent::run(this, &GuiCmd::start_instance_for, Settings::instance().get(petenv_key).toStdString()));
    });
}

void cmd::GuiCmd::update_menu()
{
    std::vector<std::string> instances_to_remove;

    auto reply = list_future.result();

    handle_petenv_instance(reply.instances());

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

    const auto petenv_name = Settings::instance().get(petenv_key).toStdString();
    for (const auto& instance : reply.instances())
    {
        auto name = instance.name();
        auto state = instance.instance_status();
        auto state_string = QString::fromStdString(mp::format::status_string_for(state));

        auto it = instances_entries.find(name);

        if (it != instances_entries.end())
        {
            auto instance_state = it->second.state;
            if (name == petenv_name || state.status() == InstanceStatus::DELETED)
            {
                instances_entries.erase(name);
            }
            else if (instance_state.status() != state.status())
            {
                auto& instance_menu = instances_entries.at(name).menu;

                if (state.status() == InstanceStatus::STOPPED)
                    instance_menu->setTitle(QString::fromStdString(name));
                else
                    instance_menu->setTitle(QString("%1 (%2)").arg(QString::fromStdString(name)).arg(state_string));
                instance_menu->clear();
                set_menu_actions_for(name, state);
                instances_entries[name].state = state;
            }

            continue;
        }

        if (name == petenv_name)
            continue;

        if (state.status() != InstanceStatus::DELETED)
        {
            instances_entries[name].menu =
                std::make_unique<QMenu>(QString("%1 (%2)").arg(QString::fromStdString(name)).arg(state_string));
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

    tray_icon.setIcon(QIcon{":images/multipass-icon.png"});

    QObject::connect(&list_watcher, &QFutureWatcher<ListReply>::finished, this, &GuiCmd::update_menu);

    QObject::connect(&menu_update_timer, &QTimer::timeout, this, [this] { initiate_menu_layout(); });

    // Use a singleShot here to make sure the event loop is running before the quit() runs
    QObject::connect(quit_action, &QAction::triggered, [this] {
        future_synchronizer.waitForFinished();
        QTimer::singleShot(0, [] { QCoreApplication::quit(); });
    });

    QObject::connect(&version_watcher, &QFutureWatcher<VersionReply>::finished, this, &GuiCmd::update_about_menu);
    QObject::connect(&about_update_timer, &QTimer::timeout, this, [this] { initiate_about_menu_layout(); });
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
        tray_icon_menu.insertAction(about_separator, &failure_action);

        return standard_failure_handler_for(name(), cerr, status);
    };

    ListRequest request;
    dispatch(&RpcMethod::list, request, on_success, on_failure);

    return list_reply;
}

void cmd::GuiCmd::set_menu_actions_for(const std::string& instance_name, const mp::InstanceStatus& state)
{
    auto& instance_menu = instances_entries.at(instance_name).menu;

    instance_menu->addAction("Open shell");
    QObject::connect(instance_menu->actions().back(), &QAction::triggered, [instance_name] {
        mp::cli::platform::open_multipass_shell(QString::fromStdString(instance_name));
    });

    if (state.status() == InstanceStatus::RUNNING)
    {
        instance_menu->addAction("Stop");
        QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
            future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::stop_instance_for, instance_name));
        });
    }
    else if (state.status() == InstanceStatus::STOPPED)
    {
        instance_menu->addAction("Start");
        QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
            future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::start_instance_for, instance_name));
        });
    }

    tray_icon_menu.insertMenu(about_separator, instance_menu.get());
}

void cmd::GuiCmd::handle_petenv_instance(const google::protobuf::RepeatedPtrField<mp::ListVMInstance>& instances)
{
    auto petenv_name = Settings::instance().get(petenv_key);

    auto petenv_instance =
        std::find_if(instances.cbegin(), instances.cend(), [&petenv_name](const ListVMInstance& instance) {
            return petenv_name.toStdString() == instance.name();
        });

    if (petenv_instance == instances.cend())
    {
        petenv_start_action.setText("Start");
        petenv_start_action.setEnabled(false);
        petenv_stop_action.setEnabled(false);

        return;
    }

    auto state = petenv_instance->instance_status();
    auto state_string = QString::fromStdString(mp::format::status_string_for(state));


    if (petenv_state.status() != state.status())
    {
        petenv_start_action.setText(QString::fromStdString(
            fmt::format("Start \"{}\"{}", petenv_name,
                        (state.status() != InstanceStatus::STOPPED) ? fmt::format(" ({})", state_string) : "")));

        set_petenv_actions_for(state);
        petenv_state = state;
    }
}

void cmd::GuiCmd::set_petenv_actions_for(const mp::InstanceStatus& state)
{
    const auto can_stop_states = {InstanceStatus::UNKNOWN, InstanceStatus::RUNNING, InstanceStatus::DELAYED_SHUTDOWN};
    const auto can_start_states = {InstanceStatus::STOPPED, InstanceStatus::SUSPENDED};

    if (std::find(can_stop_states.begin(), can_stop_states.end(), state.status()) != can_stop_states.end())
    {
        if (state.status() == InstanceStatus::UNKNOWN)
            petenv_shell_action.setEnabled(false);
        else
            petenv_shell_action.setEnabled(true);

        petenv_stop_action.setEnabled(true);
        petenv_start_action.setEnabled(false);
    }
    else if (std::find(can_start_states.begin(), can_start_states.end(), state.status()) != can_start_states.end())
    {
        petenv_shell_action.setEnabled(true);
        petenv_start_action.setEnabled(true);
        petenv_stop_action.setEnabled(false);
    }
    else
    {
        if (state.status() == InstanceStatus::DELETED || state.status() == InstanceStatus::SUSPENDING)
            petenv_shell_action.setEnabled(false);
        else
            petenv_shell_action.setEnabled(true);

        petenv_start_action.setEnabled(false);
        petenv_stop_action.setEnabled(false);
    }
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
