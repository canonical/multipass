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

#ifndef MULTIPASS_SYSTRAY_H
#define MULTIPASS_SYSTRAY_H

#include <multipass/cli/command.h>

#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

#include <memory>
#include <thread>
#include <vector>

namespace multipass
{
namespace cmd
{
class Systray final : public QObject, public Command
{
    Q_OBJECT
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;

    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser) override;
    void create_actions();
    void create_menu();
    void retrieve_all_instances();

    QSystemTrayIcon tray_icon;
    QMenu tray_icon_menu;

    QAction* retrieving_action;
    QAction* about_separator;
    QAction* about_action;
    QAction* quit_action;
    QAction failure_action{"Failure retrieving instances"};

    std::vector<std::unique_ptr<QMenu>> instances_menus;
    std::vector<QAction*> instances_actions;

    bool worker_running{false};
    std::mutex worker_mutex;
    std::thread worker;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_SYSTRAY_H
