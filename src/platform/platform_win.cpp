/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/hyperv/hyperv_virtual_machine_factory.h"
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#include "logger/win_event_logger.h"
#include "platform_proprietary.h"
#include "shared/sshfs_server_process_spec.h"
#include "shared/win/process_factory.h"
#include <default_update_prompt.h>

#include <QDir>
#include <QFile>
#include <QtGlobal>

#include <json/json.h>

#include <windows.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>

namespace mp = multipass;
namespace mpl = mp::logging;

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

QString locate_profiles_path()
{
    // The profiles file is expected in
    // $env:LocalAppData\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\profiles.json
    // where $env:LocalAppData is normally C:\Users\<USER>\AppData\Local
    return mp::StandardPaths::instance().locate(
        mp::StandardPaths::GenericConfigLocation,
        "Packages\\Microsoft.WindowsTerminal_8wekyb3d8bbwe\\LocalState\\profiles.json");
}

Json::Value& get_profiles(Json::Value& json_root)
{
    auto& profiles = json_root["profiles"];
    return profiles.isArray() ? profiles : profiles["list"];
}

} // namespace

std::map<QString, QString> mp::platform::extra_settings_defaults()
{
    return {{mp::winterm_key, {"primary"}}};
}

QString mp::platform::interpret_winterm_integration(const QString& val)
{
    static const auto acceptable = QStringList{"none", "primary"};

    auto ret = val.toLower();
    if (!acceptable.contains(ret))
        throw InvalidSettingsException{
            winterm_key, val, QStringLiteral("Unknown value. Try one of these: %1.").arg(acceptable.join(", "))};

    return ret;
}

void mp::platform::sync_winterm_profiles()
{
    constexpr auto log_category = "winterm";
    const auto profiles_path = locate_profiles_path();
    const auto winterm_setting = mp::Settings::instance().get(mp::winterm_key);
    const auto none = QStringLiteral("none");
    if (!profiles_path.isEmpty())
    {
        const auto mode = std::ifstream::binary | std::ifstream::in | std::ifstream::out;
        if (std::fstream json_file{profiles_path.toStdString(), mode})
        {
            Json::CharReaderBuilder rbuilder;
            Json::Value json_root;
            std::string errs;
            if (!Json::parseFromStream(rbuilder, json_file, &json_root, &errs))
            {
                const auto level = winterm_setting == none ? mpl::Level::info : mpl::Level::error;
                const auto error_fmt = "Could not parse Windows Terminal's configuration (located at \"{}\"); "
                                       "reason: {}";
                mpl::log(level, log_category, fmt::format(error_fmt, profiles_path, errs));
            }

            auto& profiles = get_profiles(json_root);
            auto primary_profile_it = std::find_if(std::begin(profiles), std::end(profiles), [](const auto& profile) {
                return profile["guid"] == mp::winterm_profile_guid;
            });

            if (winterm_setting == none)
            {
                ; // TODO@ricab
            }
            else if (primary_profile_it != std::end(profiles))
            {
                if (primary_profile_it->isMember("hidden") && (*primary_profile_it)["hidden"].asBool())
                    (*primary_profile_it)["hidden"] = false;
            }
            else
                ; // TODO@ricab add primary profile

            json_file.clear();
            json_file.seekg(0);
            json_file << json_root;
            if (!json_file)
            {
                const auto error_fmt = "Could not update Windows Terminal's configuration (located at \"{}\"); "
                                       "reason: {} (error code {})";
                mpl::log(mpl::Level::error, log_category,
                         fmt::format(error_fmt, profiles_path, std::strerror(errno), errno));
            }
            // TODO@ricab extract error logging? (probably through internal exceptions?)
        }
        else
        {
            const auto level = winterm_setting == none ? mpl::Level::info : mpl::Level::error;
            const auto error_fmt = "Could not read Windows Terminal's configuration (located at \"{}\"); "
                                   "reason: {} (error code {})";
            mpl::log(level, log_category, fmt::format(error_fmt, profiles_path, std::strerror(errno), errno));
        }
    }
    else if (winterm_setting != none)
        mpl::log(mpl::Level::warning, log_category, "Could not find Windows Terminal");
}

QString mp::platform::autostart_test_data()
{
    return "stub"; // TODO implement this when using setup_gui_autostart_prerequisites as the sole backend to `multipass
                   // set client.gui.autostart`
}

void mp::platform::setup_gui_autostart_prerequisites()
{
    // TODO implement this to use as the sole backend to `multipass set client.gui.autostart`
}

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
    ret =
        QDir{ret}.absoluteFilePath("Local"); // what LOCALAPPDATA would point to under the system account, at this point
    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);

    return ret; // should be something like "C:/Windows/system32/config/systemprofile/AppData/Local/multipassd"
}

bool mp::platform::is_backend_supported(const QString& backend)
{
    return backend == "hyperv" || backend == "virtualbox";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path&)
{
    const auto driver = utils::get_driver_str();

    if (driver == QStringLiteral("hyperv"))
        return std::make_unique<HyperVVirtualMachineFactory>();
    else if (driver == QStringLiteral("virtualbox"))
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

std::unique_ptr<mp::Process> mp::platform::make_sshfs_server_process(const mp::SSHFSServerConfig& config)
{
    return mp::ProcessFactory::instance().create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::EventLogger>(level);
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DefaultUpdatePrompt>();
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

int mp::platform::wait_for_quit_signals()
{
    HANDLE hSemaphore = CreateSemaphore(nullptr, 0, 128000, nullptr);

    if (hSemaphore == (HANDLE) nullptr)
        printf("Unable to create semaphore\n");

    WaitForSingleObject(hSemaphore, INFINITE); // Ctrl+C will break this wait.
    return 0;
}
