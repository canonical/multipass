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

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/settings/custom_setting_spec.h>
#include <multipass/settings/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/hyperv/hyperv_virtual_machine_factory.h"
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#include "logger/win_event_logger.h"
#include "platform_proprietary.h"
#include "platform_shared.h"
#include "shared/sshfs_server_process_spec.h"
#include "shared/windows/powershell.h"
#include "shared/windows/process_factory.h"
#include <daemon/default_vm_image_vault.h>
#include <default_update_prompt.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTemporaryFile>
#include <QtGlobal>

#include <scope_guard.hpp>

#include <json/json.h>

#include <aclapi.h>
#include <windows.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <libssh/sftp.h>

// Needed for OpenSSL dll compatibility
extern "C"
{
#include <openssl/applink.c>
}

namespace mp = multipass;
namespace mpl = mp::logging;
namespace mpu = multipass::utils;

namespace
{
static const auto none = QStringLiteral("none");

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

QString interpret_winterm_setting(const QString& val)
{
    static const auto acceptable = QStringList{none, mp::petenv_default};

    auto ret = val.toLower();
    if (!acceptable.contains(ret))
        throw mp::InvalidSettingException{
            mp::winterm_key, val, QStringLiteral("Unknown value. Try one of these: %1.").arg(acceptable.join(", "))};

    return ret;
}

QString locate_profiles_path()
{
    // The profiles file is expected in
    // $env:LocalAppData\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json
    // where $env:LocalAppData is normally C:\Users\<USER>\AppData\Local
    return MP_STDPATHS.locate(mp::StandardPaths::GenericConfigLocation,
                              "Packages/Microsoft.WindowsTerminal_8wekyb3d8bbwe/LocalState/settings.json");
}

struct WintermSyncException : public std::runtime_error
{
    WintermSyncException(const std::string& msg, const QString& path, const std::string& reason)
        : std::runtime_error{fmt::format("{}; location: \"{}\"; reason: {}.", msg, path, reason)}
    {
    }

    WintermSyncException(const std::string& msg, const QString& path)
        : WintermSyncException{msg, path, fmt::format("{} (error code: {})", std::strerror(errno), errno)}
    {
    }
};

struct LesserWintermSyncException : public WintermSyncException
{
    using WintermSyncException::WintermSyncException;
};

struct ModerateWintermSyncException : public WintermSyncException
{
    using WintermSyncException::WintermSyncException;
};

struct GreaterWintermSyncException : public WintermSyncException
{
    using WintermSyncException::WintermSyncException;
};

Json::Value& edit_profiles(const QString& path, Json::Value& json_root)
{
    if (!json_root.isMember("profiles"))
        throw ModerateWintermSyncException{"Could not find profiles in Windows Terminal's settings", path,
                                           "No \"profiles\" node under JSON root"};

    auto& profiles = json_root["profiles"]; // the array of profiles can be in this node or in the subnode "list"
    return profiles.isArray() || !profiles.isMember("list") ? profiles : profiles["list"]; /* Notes:
                                                                    1) don't index into "list" unless it already exists
                                                                    2) can't look for named member on array values */
}

Json::Value read_winterm_settings(const QString& path)
{
    std::ifstream json_file{path.toStdString(), std::ifstream::binary};
    if (!json_file)
        throw ModerateWintermSyncException{"Could not read Windows Terminal's configuration", path};

    Json::CharReaderBuilder rbuilder;
    Json::Value json_root;
    std::string errs;
    if (!Json::parseFromStream(rbuilder, json_file, &json_root, &errs))
        throw ModerateWintermSyncException{"Could not parse Windows Terminal's configuration", path, errs};

    return json_root;
}

Json::Value create_primary_profile()
{
    Json::Value primary_profile{};
    primary_profile["guid"] = mp::winterm_profile_guid;
    primary_profile["name"] = "Multipass";
    primary_profile["commandline"] = "multipass shell";
    primary_profile["background"] = "#350425";
    primary_profile["cursorShape"] = "filledBox";
    primary_profile["fontFace"] = "Ubuntu Mono";
    primary_profile["historySize"] = 50000;
    primary_profile["icon"] = QDir{QCoreApplication::applicationDirPath()}.filePath("multipass_wt.ico").toStdString();

    return primary_profile;
}

Json::Value update_profiles(const QString& path, const Json::Value& json_root, const QString& winterm_setting)
{
    Json::Value ret = json_root;
    auto& profiles = edit_profiles(path, ret);

    auto primary_profile_it = std::find_if(std::begin(profiles), std::end(profiles), [](const auto& profile) {
        return profile["guid"] == mp::winterm_profile_guid;
    });

    if (primary_profile_it != std::end(profiles))
    {
        if (primary_profile_it->isMember("hidden") || winterm_setting == none)
            (*primary_profile_it)["hidden"] = winterm_setting == none;
    }
    else if (winterm_setting != none)
        profiles.append(create_primary_profile());

    return ret;
}

void write_profiles(const std::string& path, const Json::Value& json_root)
{
    std::ofstream json_file{path, std::ofstream::binary};
    json_file << json_root;
    if (!json_file)
        throw GreaterWintermSyncException{"Could not write Windows Terminal's configuration",
                                          QString::fromStdString(path)};
}

std::string create_shadow_config_file(const QString& path)
{
    const auto tmp_file_template = path + ".XXXXXX";
    auto tmp_file = QTemporaryFile{tmp_file_template};

    if (!tmp_file.open() || !tmp_file.exists())
        throw GreaterWintermSyncException{"Could not create temporary configuration file for Windows Terminal",
                                          tmp_file_template};

    tmp_file.setAutoRemove(false);
    return tmp_file.fileName().toStdString();
}

void save_profiles(const QString& path, const Json::Value& json_root)
{
    std::string tmp_file_name = create_shadow_config_file(path);
    auto tmp_file_removing_guard = sg::make_scope_guard([&tmp_file_name]() noexcept {
        std::error_code ec; // ignored, there's an exception in flight and we're in a dtor, so best-effort only
        std::filesystem::remove(tmp_file_name, ec);
    });

    write_profiles(tmp_file_name, json_root);

    try
    {
        std::filesystem::rename(tmp_file_name, path.toStdString());
    }
    catch (std::filesystem::filesystem_error& e)
    {
        throw GreaterWintermSyncException{"Could not update Windows Terminal's configuration", path, e.what()};
    }

    tmp_file_removing_guard.dismiss(); // we succeeded, tmp file no longer there
}

std::string interpret_net_type(const QString& media_type, const QString& physical_media_type)
{
    // Note: use the following to see what types may be returned
    // `get-netadapter | select -first 1 | get-member -name physicalmediatype | select -expandproperty definition`
    if (physical_media_type == "802.3")
        return "ethernet";
    else if (physical_media_type == "Unspecified")
        return media_type == "802.3" ? "ethernet" : "unknown"; // virtio covered here
    else if (physical_media_type.contains("802.11"))
        return "wifi";
    else
        return physical_media_type.toLower().toStdString();
}

QString get_alias_script_path(const std::string& alias)
{
    auto aliases_folder = MP_PLATFORM.get_alias_scripts_folder();

    return aliases_folder.absoluteFilePath(QString::fromStdString(alias)) + ".bat";
}

QString program_data_multipass_path()
{
    return QDir{qEnvironmentVariable("ProgramData", "C:\\ProgramData")}.absoluteFilePath("Multipass");
}

QString systemprofile_app_data_path()
{
    auto ret = QString{qgetenv("SYSTEMROOT")};
    ret = QDir{ret}.absoluteFilePath("system32");
    ret = QDir{ret}.absoluteFilePath("config");
    ret = QDir{ret}.absoluteFilePath("systemprofile");
    ret = QDir{ret}.absoluteFilePath("AppData");

    return ret;
}

bool set_specific_perms(LPSTR path, PSID pSid, DWORD access_mask)
{
    PACL pOldDACL = NULL, pDACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS ea;

    GetNamedSecurityInfo(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &pOldDACL, NULL, &pSD);

    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));

    ea.grfAccessPermissions = access_mask;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPTSTR)pSid;

    SetEntriesInAcl(1, &ea, pOldDACL, &pDACL);
    auto success = SetNamedSecurityInfo(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pDACL, NULL);

    LocalFree((HLOCAL)pSD);
    LocalFree((HLOCAL)pDACL);

    return success;
}

bool set_specific_perms(LPSTR path, WELL_KNOWN_SID_TYPE sid_type, DWORD access_mask)
{
    DWORD dwSize = SECURITY_MAX_SID_SIZE;
    PSID pSid = static_cast<PSID>(LocalAlloc(LPTR, dwSize));
    CreateWellKnownSid(sid_type, nullptr, pSid, &dwSize);
    auto success = set_specific_perms(path, pSid, access_mask);
    LocalFree(pSid);

    return success;
}

DWORD convert_permissions(int unix_perms)
{
    if (unix_perms & 0x7)
        return GENERIC_ALL;

    DWORD access_mask = 0;
    if (unix_perms & 0x4)
        access_mask |= GENERIC_READ;
    if (unix_perms & 0x2)
        access_mask |= GENERIC_WRITE;
    if (unix_perms |= 0x1)
        access_mask |= GENERIC_EXECUTE;

    return access_mask;
}
} // namespace

std::map<std::string, mp::NetworkInterfaceInfo> mp::platform::Platform::get_network_interfaces_info() const
{
    static const auto ps_cmd_base = QStringLiteral(
        "Get-NetAdapter -physical | Select-Object -Property Name,MediaType,PhysicalMediaType,InterfaceDescription");
    static const auto ps_args = QString{ps_cmd_base}.split(' ', Qt::SkipEmptyParts) + PowerShell::Snippets::to_bare_csv;

    QString ps_output;
    if (PowerShell::exec(ps_args, "Network Listing on Windows Platform", &ps_output))
    {
        std::map<std::string, mp::NetworkInterfaceInfo> ret{};
        for (const auto& line : ps_output.split(QRegularExpression{"[\r\n]"}, Qt::SkipEmptyParts))
        {
            auto terms = line.split(',', Qt::KeepEmptyParts);
            if (terms.size() != 4)
            {
                throw std::runtime_error{fmt::format(
                    "Could not determine available networks - unexpected powershell output: {}", ps_output)};
            }

            auto iface = mp::NetworkInterfaceInfo{terms[0].toStdString(), interpret_net_type(terms[1], terms[2]),
                                                  terms[3].toStdString()};
            ret.emplace(iface.id, iface);
        }

        return ret;
    }

    auto detail = ps_output.isEmpty() ? "" : fmt::format(" Detail: {}", ps_output);
    auto err = fmt::format("Could not determine available networks - error executing powershell command.{}", detail);
    throw std::runtime_error{err};
}

bool mp::platform::Platform::is_alias_supported(const std::string& alias, const std::string& remote) const
{
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
            if (it->second.empty() || (it->second.find(alias) != it->second.end()))
                return true;
        }
    }

    return false;
}

bool mp::platform::Platform::is_remote_supported(const std::string& remote) const
{
    if (remote.empty() || check_unlock_code())
        return true;

    if (supported_remotes_aliases_map.find(remote) != supported_remotes_aliases_map.end())
    {
        return true;
    }

    return false;
}

bool mp::platform::Platform::is_backend_supported(const QString& backend) const
{
    return backend == "hyperv" || backend == "virtualbox";
}

void mp::platform::Platform::set_server_socket_restrictions(const std::string& /* server_address */,
                                                            const bool /* restricted */) const
{
}

QString mp::platform::interpret_setting(const QString& key, const QString& val)
{
    if (key == mp::winterm_key)
        return interpret_winterm_setting(val);
    else if (key == mp::hotkey_key)
        return mp::platform::interpret_hotkey(val);

    // this should not happen (settings should have found it to be an invalid key)
    throw InvalidSettingException(key, val, "Setting unavailable on Windows");
}

void mp::platform::sync_winterm_profiles()
{
    constexpr auto log_category = "winterm";
    const auto profiles_path = locate_profiles_path();
    const auto winterm_setting = MP_SETTINGS.get(mp::winterm_key);

    try
    {
        if (profiles_path.isEmpty())
            throw LesserWintermSyncException{"Could not find Windows Terminal's settings", profiles_path,
                                             "File not found"};

        auto json_root = read_winterm_settings(profiles_path);
        auto updated_json = update_profiles(profiles_path, json_root, winterm_setting);
        if (updated_json != json_root)
            save_profiles(profiles_path, updated_json);
    }
    catch (LesserWintermSyncException& e)
    {
        const auto level = winterm_setting == none ? mpl::Level::debug : mpl::Level::warning;
        mpl::log(level, log_category, e.what());
    }
    catch (ModerateWintermSyncException& e)
    {
        const auto level = winterm_setting == none ? mpl::Level::info : mpl::Level::error;
        mpl::log(level, log_category, e.what());
    }
    catch (GreaterWintermSyncException& e)
    {
        mpl::log(mpl::Level::error, log_category, e.what());
    }
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

QString mp::platform::Platform::default_driver() const
{
    return QStringLiteral("hyperv");
}

QString mp::platform::Platform::default_privileged_mounts() const
{
    return QStringLiteral("false");
}

bool mp::platform::Platform::is_image_url_supported() const
{
    return check_unlock_code();
}

QString mp::platform::Platform::daemon_config_home() const // temporary
{
    auto ret = systemprofile_app_data_path();
    ret =
        QDir{ret}.absoluteFilePath("Local"); // what LOCALAPPDATA would point to under the system account, at this point
    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);

    if (QFile::exists(ret))
    {
        return ret; // should be something like "C:/Windows/system32/config/systemprofile/AppData/Local/multipassd"
    }
    else
    {
        return MP_PLATFORM.multipass_storage_location();
    }
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path&)
{
    const auto driver = MP_SETTINGS.get(mp::driver_key);

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
    return MP_PROCFACTORY.create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
}

std::unique_ptr<mp::Process> mp::platform::make_process(std::unique_ptr<mp::ProcessSpec>&& process_spec)
{
    return MP_PROCFACTORY.create_process(std::move(process_spec));
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::EventLogger>(level);
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DefaultUpdatePrompt>();
}

int mp::platform::Platform::chown(const char* path, unsigned int uid, unsigned int gid) const
{
    return 0;
}

int mp::platform::Platform::chmod(const char* path, unsigned int mode) const
{
    return 0;
}

bool mp::platform::Platform::set_permissions(const multipass::Path path, const QFileDevice::Permissions perms) const
{
    LPSTR lpPath = _strdup(path.toStdString().c_str());
    auto success = true;

    SetNamedSecurityInfo(lpPath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, nullptr);

    if (perms & 0x0007)
        success &= set_specific_perms(lpPath, WinWorldSid, convert_permissions((int)(perms & 0x0007)));
    if (perms & 0x0070)
        success &= set_specific_perms(lpPath, WinCreatorGroupSid, convert_permissions((int)((perms & 0x0070) >> 4)));
    if (perms & 0x0700)
    {
        HANDLE hToken;
        DWORD pTokenDwSize = 0;
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken);
        GetTokenInformation(hToken, TokenUser, NULL, 0, &pTokenDwSize);
        PTOKEN_USER pTokenUser = static_cast<PTOKEN_USER>(LocalAlloc(LPTR, pTokenDwSize));
        GetTokenInformation(hToken, TokenUser, pTokenUser, pTokenDwSize, &pTokenDwSize);

        success &= set_specific_perms(lpPath, pTokenUser->User.Sid, convert_permissions((int)((perms & 0x0700) >> 8)));
        LocalFree(pTokenUser);
        CloseHandle(hToken);
    }
    if (perms & 0x7000)
        success &= set_specific_perms(lpPath, WinCreatorOwnerSid, convert_permissions((int)((perms & 0x7000) >> 12)));

    return success;
}

bool mp::platform::Platform::symlink(const char* target, const char* link, bool is_dir) const
{
    DWORD flags = is_dir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0x00 | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    return CreateSymbolicLink(link, target, flags);
}

bool mp::platform::Platform::link(const char* target, const char* link) const
{
    return CreateHardLink(link, target, nullptr);
}

int mp::platform::Platform::utime(const char* path, int atime, int mtime) const
{
    DWORD ret = NO_ERROR;
    auto handle = CreateFile(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);

    if (handle != INVALID_HANDLE_VALUE)
    {
        auto atime_filetime = filetime_from(atime);
        auto mtime_filetime = filetime_from(mtime);

        if (!SetFileTime(handle, nullptr, &atime_filetime, &mtime_filetime))
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

QString mp::platform::Platform::get_username() const
{
    QString username;
    mp::PowerShell::exec({"((Get-WMIObject -class Win32_ComputerSystem | Select-Object -ExpandProperty username))"},
                         "get-username", &username);
    return username.section('\\', 1);
}

QDir mp::platform::Platform::get_alias_scripts_folder() const
{
    QDir aliases_folder;

    QString location = MP_STDPATHS.writableLocation(mp::StandardPaths::HomeLocation) + "/AppData/local/multipass/bin";
    aliases_folder = QDir{location};

    if (!aliases_folder.mkpath(aliases_folder.path()))
        throw std::runtime_error(fmt::format("error creating \"{}\"\n", aliases_folder.path()));

    return aliases_folder;
}

void mp::platform::Platform::create_alias_script(const std::string& alias, const mp::AliasDefinition& def) const
{
    auto file_path = get_alias_script_path(alias);

    std::string multipass_exec = QCoreApplication::applicationFilePath().toStdString();

    std::string script = "@\"" + multipass_exec + "\" " + alias + " -- %*\n";

    MP_UTILS.make_file_with_content(file_path.toStdString(), script, true);
}

void mp::platform::Platform::remove_alias_script(const std::string& alias) const
{
    auto file_path = get_alias_script_path(alias);

    if (!QFile::remove(file_path))
        throw std::runtime_error("error removing alias script");
}

auto mp::platform::Platform::extra_daemon_settings() const -> SettingSpec::Set
{
    return {};
}

auto mp::platform::Platform::extra_client_settings() const -> SettingSpec::Set
{
    SettingSpec::Set ret;
    ret.insert(std::make_unique<CustomSettingSpec>(
        winterm_key, petenv_default, [](const QString& val) { return interpret_setting(winterm_key, val); }));

    return ret;
}

std::string mp::platform::Platform::alias_path_message() const
{
    return fmt::format("You'll need to add the script alias folder to your path for aliases to work\n"
                       "without prefixing with `multipass`. For now, you can just do:\n\n"
                       "In PowerShell:\n$ENV:PATH=\"$ENV:PATH;{0}\"\n\n"
                       "Or in Command Prompt:\nPATH=%PATH%;{0}\n",
                       get_alias_scripts_folder().absolutePath());
}

QString mp::platform::Platform::multipass_storage_location() const
{
    auto storage_location = mp::utils::get_multipass_storage();

    // If MULTIPASS_STORAGE env var is set, use that
    if (!storage_location.isEmpty())
    {
        return storage_location;
    }

    auto program_data_path = program_data_multipass_path();
    auto systemprofile_roaming_path = QDir{systemprofile_app_data_path()}.absoluteFilePath("Roaming");

    // If %PROGRAMDATA%\Multipass exists or if %SYSTEMROOT%\system32\config\AppData\Roaming\multipassd doesn't
    // exist, use %PROGRAMDATA%\Multipass
    if (QFile::exists(program_data_path) ||
        !QFile::exists(QDir{systemprofile_roaming_path}.absoluteFilePath("multipassd")))
    {
        return program_data_path;
    }

    // If %SYSTEMROOT%\system32\config\AppData\Roaming\multipassd exists, return empty and let the
    // caller use Qt's StandardPaths to figure it out (legacy)
    return QString();
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

std::function<int()> mp::platform::make_quit_watchdog()
{
    return [hSemaphore = CreateSemaphore(nullptr, 0, 128000, nullptr)]() {
        if (hSemaphore == (HANDLE) nullptr)
            printf("Unable to create semaphore\n");

        WaitForSingleObject(hSemaphore, INFINITE); // Ctrl+C will break this wait.
        return 0;
    };
}

std::string mp::platform::reinterpret_interface_id(const std::string& ux_id)
{
    auto ps_cmd = QStringLiteral("Get-NetAdapter -Name \"%1\" | Select-Object -ExpandProperty InterfaceDescription")
                      .arg(QString::fromStdString(ux_id))
                      .split(' ', Qt::SkipEmptyParts);

    QString ps_output;
    if (PowerShell::exec(ps_cmd, "Adapter description from name", &ps_output))
    {
        auto output_lines = ps_output.split(QRegularExpression{"[\r\n]"}, Qt::SkipEmptyParts);
        if (output_lines.size() != 1)
        {
            throw std::runtime_error{
                fmt::format("Could not obtain adapter description from name \"{}\" - unexpected powershell output: {}",
                            ux_id, ps_output)};
        }

        return output_lines.first().toStdString();
    }

    auto detail = ps_output.isEmpty() ? "" : fmt::format(" Detail: {}", ps_output);
    auto err = fmt::format(
        "Could not obtain adapter description from name \"{}\" - error executing powershell command.{}", ux_id, detail);
    throw std::runtime_error{err};
}
