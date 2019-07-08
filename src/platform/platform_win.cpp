/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#include <multipass/constants.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/hyperv/hyperv_virtual_machine_factory.h"
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#include "logger/win_event_logger.h"
#include "platform_proprietary.h"
#include <github_update_prompt.h>

#include <QDir>
#include <QFile>
#include <QtGlobal>

#include <windows.h>

namespace mp = multipass;

namespace
{
time_t time_t_from(const FILETIME* ft)
{
    long long win_time = (static_cast<long long>(ft->dwHighDateTime) << 32) + ft->dwLowDateTime;
    win_time -= 116444736000000000LL;
    win_time /= 10000000;
    return static_cast<time_t>(win_time);
}

FILETIME filetime_from(const time_t t)
{
    FILETIME ft;
    auto win_time = Int32x32To64(t, 10000000) + 116444736000000000;
    ft.dwLowDateTime = static_cast<DWORD>(win_time);
    ft.dwHighDateTime = win_time >> 32;

    return ft;
}

sftp_attributes_struct stat_to_attr(const WIN32_FILE_ATTRIBUTE_DATA* data)
{
    sftp_attributes_struct attr{};

    attr.uid = -2;
    attr.gid = -2;

    attr.flags =
        SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_ACMODTIME;

    attr.atime = time_t_from(&(data->ftLastAccessTime));
    attr.mtime = time_t_from(&(data->ftLastWriteTime));

    attr.permissions = SSH_S_IFLNK | 0777;

    return attr;
}
} // namespace

std::string mp::platform::default_server_address()
{
    return {"localhost:50051"};
}

QString mp::platform::default_driver()
{
    return QStringLiteral("hyperv");
}

QString mp::platform::daemon_config_home() // temporary
{
    auto ret = QString{qgetenv("SYSTEMROOT")};
    ret = QDir{ret}.absoluteFilePath("system32");
    ret = QDir{ret}.absoluteFilePath("config");
    ret = QDir{ret}.absoluteFilePath("systemprofile");
    ret = QDir{ret}.absoluteFilePath("AppData");
    ret = QDir{ret}.absoluteFilePath("Local"); // what LOCALAPPDATA would point to under the system account, at this point
    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);

    return ret; // should be something like "C:/Windows/system32/config/systemprofile/AppData/Local/multipassd"
}

bool mp::platform::is_backend_supported(const QString& backend)
{
    return backend == "hyperv" || backend == "virtualbox";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path&) // TODO
{
    auto driver = qgetenv("MULTIPASS_VM_DRIVER");

    if (driver.isEmpty() || driver == "HYPERV")
        return std::make_unique<HyperVVirtualMachineFactory>();
    else if (driver == "VIRTUALBOX")
    {
        qputenv("Path", qgetenv("Path") + ";C:\\Program Files\\Oracle\\VirtualBox"); /*
          This is where the Virtualbox installer puts things, and relying on PATH
          allows the user to do something about it, if the binaries are not found
          there.
        */

        return std::make_unique<VirtualBoxVirtualMachineFactory>();
    }

    throw std::runtime_error("Invalid virtualization driver set in the environment");
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::EventLogger>(level);
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<GithubUpdatePrompt>();
}

int mp::platform::chown(const char* path, unsigned int uid, unsigned int gid)
{
    return 0;
}

bool mp::platform::symlink(const char* target, const char* link, bool is_dir)
{
    DWORD flags = is_dir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0x00 | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    return CreateSymbolicLink(link, target, flags);
}

bool mp::platform::link(const char* target, const char* link)
{
    return CreateHardLink(link, target, nullptr);
}

int mp::platform::utime(const char* path, int atime, int mtime)
{
    DWORD ret = NO_ERROR;
    auto handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);

    if (handle != INVALID_HANDLE_VALUE)
    {
        if (!SetFileTime(handle, nullptr, &(filetime_from(atime)), &(filetime_from(mtime))))
        {
            ret = GetLastError();
        }
        CloseHandle(handle);
    }
    else
    {
        ret = GetLastError();
    }

    return ret;
}

int mp::platform::symlink_attr_from(const char* path, sftp_attributes_struct* attr)
{
    WIN32_FILE_ATTRIBUTE_DATA data;

    if (GetFileAttributesEx(path, GetFileExInfoStandard, &data))
    {
        *attr = stat_to_attr(&data);
        attr->size = QFile::symLinkTarget(path).size();
    }

    return 0;
}

bool mp::platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    // Minimal images that the snapcraft remote uses do not work on Windows
    if (remote == "snapcraft")
        return false;

    if (check_unlock_code())
        return true;

    if (remote.empty())
    {
        if (supported_release_aliases.find(alias) != supported_release_aliases.end())
            return true;
    }
    else
    {
        auto it = supported_remotes_aliases_map.find(remote);

        if (it != supported_remotes_aliases_map.end())
        {
            if (it->second.find(alias) != it->second.end())
                return true;
        }
    }

    return false;
}

bool mp::platform::is_remote_supported(const std::string& remote)
{
    // Minimal images that the snapcraft remote uses do not work on Windows
    if (remote == "snapcraft")
        return false;

    if (remote.empty() || check_unlock_code())
        return true;

    if (supported_remotes_aliases_map.find(remote) != supported_remotes_aliases_map.end())
    {
        return true;
    }

    return false;
}

bool mp::platform::is_image_url_supported()
{
    return check_unlock_code();
}
