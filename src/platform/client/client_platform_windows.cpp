/*
 * Copyright (C) Canonical, Ltd.
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
#include <multipass/cli/prompters.h>
#include <multipass/terminal.h>
#include <shared/windows/powershell.h>

#include <QFileInfo>
#include <QProcess>
#include <QString>

#include <fcntl.h>
#include <io.h>
#include <windows.h>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;

void mcp::parse_transfer_entry(const QString& entry, QString& path, QString& instance_name)
{
    auto colon_count = entry.count(":");

    switch (colon_count)
    {
    case 0:
        path = entry;
        break;
    case 1:
        if (!QFileInfo::exists(entry) && !entry.contains(":\\"))
        {
            instance_name = entry.section(":", 0, 0);
            path = entry.section(":", 1);
        }
        else
        {
            path = entry;
        }
        break;
    }
}

int mcp::getuid()
{
    return mp::no_id_info_available;
}

int mcp::getgid()
{
    return mp::no_id_info_available;
}

void mcp::open_multipass_shell(const QString& instance_name)
{
    QProcess::startDetached(
        "cmd", {"/c", "start", "PowerShell", "-NoLogo", "-Command", QString("multipass shell %1").arg(instance_name)});
}

QStringList mcp::gui_tray_notification_strings()
{
    return {"Multipass is in your Notification area", "Right-click on the icon in the taskbar for available options"};
}

std::string mcp::Platform::get_password(mp::Terminal* term) const
{
    if (term->is_live())
    {
        mp::PassphrasePrompter prompter(term);
        auto password = prompter.prompt("Please enter your user password to allow Windows mounts");

        return password;
    }

    return {};
}

void mcp::Platform::enable_ansi_escape_chars() const
{
    HANDLE handleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD consoleMode;

    GetConsoleMode(handleOut, &consoleMode);
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handleOut, consoleMode);
}
