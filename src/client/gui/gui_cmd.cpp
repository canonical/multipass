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

#include <QHotkey>

#include <QApplication>
#include <QDesktopServices>
#include <QStyle>
#include <QtConcurrent/QtConcurrent>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;
using namespace std::chrono_literals;

namespace
{
auto set_title_string_for(const std::string& text, const mp::InstanceStatus& state)
{
    return QString::fromStdString(fmt::format("{}{}", text,
                                              (state.status() != mp::InstanceStatus::STOPPED)
                                                  ? fmt::format(" ({})", mp::format::status_string_for(state))
                                                  : ""));
}

// actions are in the following order:
// - Start action
// - Open Shell action
// - Stop action
void set_input_state_for(QList<QAction*> actions, const mp::InstanceStatus& state)
{
    if (actions.isEmpty())
        return;

    enum ActionType
    {
        start,
        open_shell,
        stop
    };

    switch (state.status())
    {
    case mp::InstanceStatus::UNKNOWN:
        actions[ActionType::start]->setEnabled(false);
        actions[ActionType::open_shell]->setEnabled(false);
        actions[ActionType::stop]->setEnabled(true);
        break;
    case mp::InstanceStatus::RUNNING:
    case mp::InstanceStatus::DELAYED_SHUTDOWN:
        actions[ActionType::start]->setEnabled(false);
        actions[ActionType::open_shell]->setEnabled(true);
        actions[ActionType::stop]->setEnabled(true);
        break;
    case mp::InstanceStatus::STOPPED:
    case mp::InstanceStatus::SUSPENDED:
        actions[ActionType::start]->setEnabled(true);
        actions[ActionType::open_shell]->setEnabled(true);
        actions[ActionType::stop]->setEnabled(false);
        break;
    case mp::InstanceStatus::DELETED:
    case mp::InstanceStatus::SUSPENDING:
        actions[ActionType::start]->setEnabled(false);
        actions[ActionType::open_shell]->setEnabled(false);
        actions[ActionType::stop]->setEnabled(false);
        break;
    default:
        actions[ActionType::start]->setEnabled(false);
        actions[ActionType::open_shell]->setEnabled(true);
        actions[ActionType::stop]->setEnabled(false);
    }
}
} // namespace

mp::ReturnCode cmd::GuiCmd::run(mp::ArgParser* parser)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        cerr << "System tray not supported\n";
        return ReturnCode::CommandFail;
    }

    auto hotkey = new QHotkey(QKeySequence("ctrl+alt+U"), true, qApp);
    if (!hotkey->isRegistered())
    {
        cerr << "Failed to register hotkey.\n";
    }

    QObject::connect(hotkey, &QHotkey::activated, qApp, [&]() { mp::cli::platform::open_multipass_shell(QString()); });

    create_actions();
    create_menu();
    tray_icon.show();

    QFile first_run_file(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/first_run");

    if (!first_run_file.exists())
    {
        // Each platform refers to the "system tray", icons, and the "menu bar" by different terminology.
        // A platform dependent mechanism is used to get the messages via a QStringList.
        auto notification_area_strings = mp::cli::platform::gui_tray_notification_strings();
        tray_icon.showMessage(notification_area_strings[0], notification_area_strings[1], tray_icon.icon());
        first_run_file.open(QIODevice::WriteOnly);
        first_run_file.close();
    }

    return static_cast<ReturnCode>(QCoreApplication::exec());
}

void cmd::GuiCmd::create_actions()
{
    auto client_config_path = Settings::get_client_settings_file_path();

    mp::utils::check_and_create_config_file(client_config_path);
    config_watcher.addPath(client_config_path);
    QObject::connect(&config_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& path) {
        autostart_option.setChecked(Settings::instance().get_as<bool>(autostart_key));
        // Needed since the original watched file may be removed and opened as a new file
        if (!config_watcher.files().contains(path) && QFile::exists(path))
        {
            config_watcher.addPath(path);
        }
    });

    about_separator = tray_icon_menu.addSeparator();
    quit_action = tray_icon_menu.addAction("Quit");

    petenv_actions_separator = tray_icon_menu.insertSeparator(tray_icon_menu.actions().first());
    tray_icon_menu.insertActions(petenv_actions_separator,
                                 {&petenv_start_action, &petenv_shell_action, &petenv_stop_action});

    QObject::connect(&petenv_shell_action, &QAction::triggered,
                     [] { mp::cli::platform::open_multipass_shell(QString()); });
    QObject::connect(&petenv_stop_action, &QAction::triggered, [this] {
        future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::stop_instance_for, current_petenv_name));
    });
    QObject::connect(&petenv_start_action, &QAction::triggered, [this] {
        future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::start_instance_for, current_petenv_name));
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

    for (const auto& instance : reply.instances())
    {
        auto name = instance.name();
        auto state = instance.instance_status();

        auto it = instances_entries.find(name);

        if (it != instances_entries.end())
        {
            auto instance_state = it->second.state;
            if (name == current_petenv_name || state.status() == InstanceStatus::DELETED)
            {
                instances_entries.erase(name);
            }
            else if (instance_state.status() != state.status())
            {
                auto& instance_menu = instances_entries.at(name).menu;

                instance_menu->setTitle(set_title_string_for(name, state));
                set_input_state_for(instance_menu->actions(), state);
                instances_entries[name].state = state;
            }

            continue;
        }

        if (name == current_petenv_name)
            continue;

        if (state.status() != InstanceStatus::DELETED)
        {
            create_menu_actions_for(name, state);
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
        update_action.setIcon(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation));
        update_action.setWhatsThis(QString::fromStdString(reply.update_info().url()));

        QObject::connect(&tray_icon, &QSystemTrayIcon::messageClicked,
                         [this] { QDesktopServices::openUrl(QUrl(update_action.whatsThis())); });

        tray_icon_menu.insertAction(about_menu.menuAction(), &update_action);
        tray_icon.showMessage(QString::fromStdString(reply.update_info().title()),
                              QString("%1\n\nClick here for more information.")
                                  .arg(QString::fromStdString(reply.update_info().description())));
    }
    else
    {
        QObject::disconnect(&tray_icon, &QSystemTrayIcon::messageClicked, 0, 0);
        tray_icon_menu.removeAction(&update_action);
    }
}

void cmd::GuiCmd::create_menu()
{
    tray_icon.setContextMenu(&tray_icon_menu);

#ifdef Q_OS_MACOS
    QIcon icon{":images/multipass-menubar-icon-mac.png"};
    icon.setIsMask(true);
#else
    QIcon icon{":images/multipass-icon.png"};
#endif
    tray_icon.setIcon(icon);

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

    autostart_option.setCheckable(true);
    autostart_option.setChecked(Settings::instance().get_as<bool>(autostart_key));
    QObject::connect(&autostart_option, &QAction::toggled, this,
                     [](bool checked) { Settings::instance().set(autostart_key, QVariant(checked).toString()); });

    about_client_version.setEnabled(false);
    about_daemon_version.setEnabled(false);
    about_copyright.setText("Copyright © 2017-2019 Canonical Ltd.");
    about_copyright.setEnabled(false);

    about_menu.insertActions(0, {&autostart_option, &about_client_version, &about_daemon_version, &about_copyright});

    tray_icon_menu.insertMenu(quit_action, &about_menu);

    initiate_menu_layout();
    initiate_about_menu_layout();

    menu_update_timer.start(1s);
    about_update_timer.start(24h);
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

void cmd::GuiCmd::create_menu_actions_for(const std::string& instance_name, const mp::InstanceStatus& state)
{
    auto& instance_menu = instances_entries[instance_name].menu =
        std::make_unique<QMenu>(set_title_string_for(instance_name, state));

    instance_menu->addAction("Start");
    QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
        future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::start_instance_for, instance_name));
    });

    instance_menu->addAction("Open Shell");
    QObject::connect(instance_menu->actions().back(), &QAction::triggered, [instance_name] {
        mp::cli::platform::open_multipass_shell(QString::fromStdString(instance_name));
    });

    instance_menu->addAction("Stop");
    QObject::connect(instance_menu->actions().back(), &QAction::triggered, [this, instance_name] {
        future_synchronizer.addFuture(QtConcurrent::run(this, &GuiCmd::stop_instance_for, instance_name));
    });

    set_input_state_for(instance_menu->actions(), state);

    tray_icon_menu.insertMenu(about_separator, instance_menu.get());
}

void cmd::GuiCmd::handle_petenv_instance(const google::protobuf::RepeatedPtrField<mp::ListVMInstance>& instances)
{
    auto petenv_name = Settings::instance().get(petenv_key).toStdString();
    auto petenv_instance =
        std::find_if(instances.cbegin(), instances.cend(),
                     [&petenv_name](const ListVMInstance& instance) { return petenv_name == instance.name(); });

    // petenv doesn't exist yet
    if (petenv_instance == instances.cend())
    {
        if (current_petenv_name != petenv_name)
        {
            petenv_start_action.setText("Start");
            petenv_start_action.setEnabled(false);
            petenv_shell_action.setEnabled(true);
            petenv_stop_action.setEnabled(false);

            current_petenv_name = petenv_name;
        }

        return;
    }

    auto state = petenv_instance->instance_status();

    if (petenv_state.status() != state.status() || petenv_name != current_petenv_name)
    {
        petenv_start_action.setText(set_title_string_for(fmt::format("Start \"{}\"", petenv_name), state));

        set_input_state_for({&petenv_start_action, &petenv_shell_action, &petenv_stop_action}, state);
        petenv_state = state;
        current_petenv_name = petenv_name;
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
