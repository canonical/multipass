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

#include <multipass/cli/client_platform.h>

#include <fmt/format.h>

#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QProcess>

#include <iostream>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;

void mcp::open_multipass_shell(const QString& instance_name)
{
    QTemporaryFile file(QDir::tempPath() + "/multipass-gui.XXXXXX.command");
    file.setAutoRemove(false);

    if (file.open())
    {
        file.write("clear\n");
        file.write(fmt::format("multipass shell {}\n", instance_name.toStdString()).c_str());
        file.close();
        file.setPermissions(QFile::ReadUser | QFile::ExeUser);

        QProcess open;
        open.setProcessChannelMode(QProcess::ForwardedChannels);
        open.start("open", {file.fileName()});
        auto success = open.waitForFinished();
        if (!success || open.exitStatus() != QProcess::NormalExit || open.exitCode() != 0)
        {
            std::cout << "Failed to run command: " << open.errorString().toStdString() << std::endl;
        }
    }
}

QStringList mcp::gui_tray_notification_strings()
{
    return {"Multipass is in your System menu", "Click on the icon in the menu bar for available options"};
}
