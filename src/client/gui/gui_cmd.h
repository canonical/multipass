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

#ifndef MULTIPASS_GUI_CMD_H
#define MULTIPASS_GUI_CMD_H

#include <multipass/cli/command.h>

#include <QFutureSynchronizer>
#include <QFutureWatcher>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>

#include <memory>
#include <unordered_map>
#include <vector>

namespace multipass
{
class ArgParser;

namespace cmd
{
class GuiCmd final : public QObject, public Command
{
    Q_OBJECT
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;

    std::string name() const override
    {
        return "";
    };

    QString short_help() const override
    {
        return "";
    };

    QString description() const override
    {
        return "";
    };

private:
    ParseCode parse_args(ArgParser* parser) override
    {
        return ParseCode::Ok;
    };

    void create_actions();
    void create_menu();
    void update_menu();
    void update_about_menu();
    void initiate_menu_layout();
    void initiate_about_menu_layout();
    ListReply retrieve_all_instances();
    void set_menu_actions_for(const std::string& instance_name, const InstanceStatus& state);
    void handle_petenv_instance(const google::protobuf::RepeatedPtrField<ListVMInstance>&);
    void set_petenv_actions_for(const InstanceStatus& state);
    void start_instance_for(const std::string& instance_name);
    void stop_instance_for(const std::string& instance_name);
    void suspend_instance_for(const std::string& instance_name);
    VersionReply retrieve_version_and_update_info();

    QSystemTrayIcon tray_icon;
    QMenu tray_icon_menu;

    QAction petenv_start_action;
    QAction petenv_shell_action{"Open Shell"};
    QAction petenv_stop_action{"Stop"};
    InstanceStatus petenv_state;
    std::string current_petenv_name;

    QAction* petenv_actions_separator;
    QAction* about_separator;
    QAction* quit_action;
    QAction update_action{"Update available"};
    QAction failure_action{"Failure retrieving instances"};

    QMenu about_menu;
    QAction about_client_version;
    QAction about_daemon_version;
    QAction about_copyright;

    struct InstanceEntry
    {
        InstanceStatus state;
        std::unique_ptr<QMenu> menu;
    };
    std::unordered_map<std::string, InstanceEntry> instances_entries;

    QFuture<ListReply> list_future;
    QFutureWatcher<ListReply> list_watcher;

    QFuture<VersionReply> version_future;
    QFutureWatcher<VersionReply> version_watcher;

    QFutureSynchronizer<void> future_synchronizer;

    QTimer menu_update_timer;
    QTimer about_update_timer;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_GUI_CMD_H
