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

#include <windows.h>
#include <fcntl.h>
#include <io.h>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;

namespace
{
uint32_t sid_to_rid(PSID sid)
{
    if (!sid || !IsValidSid(sid))
        return 0;
    PUCHAR count = GetSidSubAuthorityCount(sid);
    if (!count || *count == 0)
        return 0;
    PDWORD rid = GetSidSubAuthority(sid, *count - 1);
    return rid ? static_cast<uint32_t>(*rid) : 0;
}
}; // namespace

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
    // HANDLE hToken = nullptr;
    // if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    //     return -1;
    // auto token_guard = std::unique_ptr<void, decltype(&CloseHandle)>(hToken, CloseHandle);

    // DWORD size = 0;
    // GetTokenInformation(hToken, TokenOwner, nullptr, 0, &size);
    // std::vector<BYTE> buf(size);
    // if (!GetTokenInformation(hToken, TokenOwner, buf.data(), size, &size))
    //     return -1;

    // auto* token_owner = reinterpret_cast<TOKEN_OWNER*>(buf.data());
    // return static_cast<int>(sid_to_rid(token_owner->Owner));
    return mp::no_id_info_available;
}

int mcp::getgid()
{
    // HANDLE hToken = nullptr;
    // if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    //     return -1;
    // auto token_guard = std::unique_ptr<void, decltype(&CloseHandle)>(hToken, CloseHandle);

    // DWORD size = 0;
    // GetTokenInformation(hToken, TokenPrimaryGroup, nullptr, 0, &size);
    // std::vector<BYTE> buf(size);
    // if (!GetTokenInformation(hToken, TokenPrimaryGroup, buf.data(), size, &size))
    //     return -1;

    // auto* token_group = reinterpret_cast<TOKEN_PRIMARY_GROUP*>(buf.data());
    // return static_cast<int>(sid_to_rid(token_group->PrimaryGroup));
    return mp::no_id_info_available;
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
