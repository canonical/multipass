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
#include <sddl.h>
#include <shlobj_core.h>
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
static constexpr auto kLogCategory = "platform-win";

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

    attr.flags = SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_UIDGID | SSH_FILEXFER_ATTR_PERMISSIONS |
                 SSH_FILEXFER_ATTR_ACMODTIME;

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
            mp::winterm_key,
            val,
            QStringLiteral("Unknown value. Try one of these: %1.").arg(acceptable.join(", "))};

    return ret;
}

QString locate_profiles_path()
{
    // The profiles file is expected in
    // $env:LocalAppData\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState\settings.json
    // where $env:LocalAppData is normally C:\Users\<USER>\AppData\Local
    return MP_STDPATHS.locate(
        mp::StandardPaths::GenericConfigLocation,
        "Packages/Microsoft.WindowsTerminal_8wekyb3d8bbwe/LocalState/settings.json");
}

struct WintermSyncException : public std::runtime_error
{
    WintermSyncException(const std::string& msg, const QString& path, const std::string& reason)
        : std::runtime_error{fmt::format("{}; location: \"{}\"; reason: {}.", msg, path, reason)}
    {
    }

    WintermSyncException(const std::string& msg, const QString& path)
        : WintermSyncException{msg,
                               path,
                               fmt::format("{} (error code: {})", std::strerror(errno), errno)}
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
        throw ModerateWintermSyncException{"Could not find profiles in Windows Terminal's settings",
                                           path,
                                           "No \"profiles\" node under JSON root"};

    auto& profiles =
        json_root["profiles"]; // the array of profiles can be in this node or in the subnode "list"
    return profiles.isArray() || !profiles.isMember("list")
               ? profiles
               : profiles["list"]; /* Notes:
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
        throw ModerateWintermSyncException{"Could not parse Windows Terminal's configuration",
                                           path,
                                           errs};

    return json_root;
}

// work around white showing as pale-green, unless the user has a custom value
void wt_patch_colors(Json::Value& primary_profile)
{
    static constexpr auto off_white = "#FDFDFD";
    static constexpr auto comment_placement = Json::commentAfterOnSameLine;

    if (auto& cursor_color = primary_profile["cursorColor"]; cursor_color.isNull())
    {
        cursor_color = off_white;
        cursor_color.setComment("// work around white showing as pale-green", comment_placement);

        // match cursor color in the foreground if there is no custom value
        if (auto& foreground = primary_profile["foreground"]; foreground.isNull())
        {
            foreground = off_white;
            foreground.setComment("// match cursor", comment_placement);
        }
    }
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
    primary_profile["icon"] =
        QDir{QCoreApplication::applicationDirPath()}.filePath("multipass_wt.ico").toStdString();

    return primary_profile;
}

Json::Value update_profiles(const QString& path,
                            const Json::Value& json_root,
                            const QString& winterm_setting)
{
    Json::Value ret = json_root;
    auto& profiles = edit_profiles(path, ret);

    auto primary_profile_it =
        std::find_if(std::begin(profiles), std::end(profiles), [](const auto& profile) {
            return profile["guid"] == mp::winterm_profile_guid;
        });

    Json::Value* primary_profile_ptr = nullptr;
    if (primary_profile_it != std::end(profiles))
    {
        if (primary_profile_it->isMember("hidden") || winterm_setting == none)
            (*primary_profile_it)["hidden"] = winterm_setting == none;
        primary_profile_ptr = &(*primary_profile_it);
    }
    else if (winterm_setting != none)
        primary_profile_ptr = &profiles.append(create_primary_profile());

    if (primary_profile_ptr)
        wt_patch_colors(*primary_profile_ptr);

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
        throw GreaterWintermSyncException{
            "Could not create temporary configuration file for Windows Terminal",
            tmp_file_template};

    tmp_file.setAutoRemove(false);
    return tmp_file.fileName().toStdString();
}

void save_profiles(const QString& path, const Json::Value& json_root)
{
    std::string tmp_file_name = create_shadow_config_file(path);
    auto tmp_file_removing_guard = sg::make_scope_guard([&tmp_file_name]() noexcept {
        std::error_code
            ec; // ignored, there's an exception in flight and we're in a dtor, so best-effort only
        std::filesystem::remove(tmp_file_name, ec);
    });

    write_profiles(tmp_file_name, json_root);

    try
    {
        std::filesystem::rename(tmp_file_name, path.toStdString());
    }
    catch (std::filesystem::filesystem_error& e)
    {
        throw GreaterWintermSyncException{"Could not update Windows Terminal's configuration",
                                          path,
                                          e.what()};
    }

    tmp_file_removing_guard.dismiss(); // we succeeded, tmp file no longer there
}

std::string interpret_net_type(const QString& media_type, const QString& physical_media_type)
{
    // Note: use the following to see what types may be returned
    // `get-netadapter | select -first 1 | get-member -name physicalmediatype | select
    // -expandproperty definition`
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
    return QDir{qEnvironmentVariable("ProgramData", "C:\\ProgramData")}.absoluteFilePath(
        "Multipass");
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

[[nodiscard]] mp::fs::path get_wellknown_path(REFKNOWNFOLDERID rfid)
{
    PWSTR out = nullptr;
    auto guard = sg::make_scope_guard([&out]() noexcept { ::CoTaskMemFree(out); });

    if (auto err = SHGetKnownFolderPath(rfid, KF_FLAG_DEFAULT, NULL, &out); err != S_OK)
    {
        throw std::system_error(err, std::system_category(), "Failed to get well known path");
    }

    return out;
}

DWORD set_privilege(HANDLE handle, LPCTSTR privilege, bool enable)
{
    LUID id;
    if (!LookupPrivilegeValue(NULL, privilege, &id))
    {
        return GetLastError();
    }

    TOKEN_PRIVILEGES privileges{};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = id;
    privileges.Privileges[0].Attributes = (enable) ? SE_PRIVILEGE_ENABLED : 0;

    if (!AdjustTokenPrivileges(handle, false, &privileges, sizeof(privileges), NULL, NULL))
    {
        return GetLastError();
    }

    return (GetLastError() == ERROR_NOT_ALL_ASSIGNED) ? ERROR_NOT_ALL_ASSIGNED : ERROR_SUCCESS;
}

DWORD set_file_owner(LPSTR path, PSID new_owner)
{
    std::unique_ptr<void, decltype(&CloseHandle)> token(nullptr, CloseHandle);
    if (HANDLE t; !OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &t))
    {
        return GetLastError();
    }
    else
    {
        token.reset(t);
    }

    if (auto err = set_privilege(token.get(), SE_TAKE_OWNERSHIP_NAME, true); err != ERROR_SUCCESS)
    {
        return err;
    }

    auto r = SetNamedSecurityInfo(path,
                                  SE_FILE_OBJECT,
                                  OWNER_SECURITY_INFORMATION,
                                  new_owner,
                                  NULL,
                                  NULL,
                                  NULL);
    set_privilege(token.get(), SE_TAKE_OWNERSHIP_NAME, false);
    return r;
}

DWORD set_specific_perms(LPSTR path, PSID pSid, DWORD access_mask, bool inherit)
{
    PACL pOldDACL = NULL, pDACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS ea;

    GetNamedSecurityInfo(path,
                         SE_FILE_OBJECT,
                         DACL_SECURITY_INFORMATION,
                         NULL,
                         NULL,
                         &pOldDACL,
                         NULL,
                         &pSD);

    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));

    ea.grfAccessPermissions = access_mask;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName = (LPTSTR)pSid;

    SetEntriesInAcl(1, &ea, pOldDACL, &pDACL);
    auto error_code = SetNamedSecurityInfo(path,
                                           SE_FILE_OBJECT,
                                           DACL_SECURITY_INFORMATION |
                                               ((inherit) ? UNPROTECTED_DACL_SECURITY_INFORMATION
                                                          : PROTECTED_DACL_SECURITY_INFORMATION),
                                           NULL,
                                           NULL,
                                           pDACL,
                                           NULL);

    LocalFree((HLOCAL)pSD);
    LocalFree((HLOCAL)pDACL);

    return error_code;
}

// returns a PSID unique_ptr for a well known pid
auto get_well_known_sid(WELL_KNOWN_SID_TYPE type)
{
    DWORD len = SECURITY_MAX_SID_SIZE;
    auto sid = std::unique_ptr<void, decltype(&FreeSid)>(LocalAlloc(LPTR, len), FreeSid);
    if (!CreateWellKnownSid(type, nullptr, sid.get(), &len))
    {
        throw std::system_error(GetLastError(),
                                std::system_category(),
                                "Failed to create well known SID");
    }

    return sid;
}

DWORD set_specific_perms(LPSTR path, WELL_KNOWN_SID_TYPE sid_type, DWORD access_mask, bool inherit)
{
    auto pSid = get_well_known_sid(sid_type);
    return set_specific_perms(path, pSid.get(), access_mask, inherit);
}

DWORD convert_permissions(int unix_perms)
{
    if (unix_perms == 07)
        return GENERIC_ALL;

    DWORD access_mask = 0;
    if (unix_perms & 04)
        access_mask |= GENERIC_READ;
    if (unix_perms & 02)
        access_mask |= GENERIC_WRITE;
    if (unix_perms & 01)
        access_mask |= GENERIC_EXECUTE;

    return access_mask;
}

// Since the Windows API expects a C style function pointer, we cannot pass parameters.
// This means that only one watchdog can exist at a time.
HANDLE signal_event = (HANDLE) nullptr;

BOOL signal_handler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
        // In Windows, ctrl-break is always delivered to all
        // registered handlers no matter what the previous
        // handlers return.
        [[fallthrough]];
    case CTRL_BREAK_EVENT:
    {
        SetEvent(signal_event);
        return TRUE;
    }
    default:
        return FALSE;
    }
}
} // namespace

std::map<std::string, mp::NetworkInterfaceInfo>
mp::platform::Platform::get_network_interfaces_info() const
{
    static const auto ps_cmd_base =
        QStringLiteral("Get-NetAdapter -physical | Select-Object -Property "
                       "Name,MediaType,PhysicalMediaType,InterfaceDescription");
    static const auto ps_args =
        QString{ps_cmd_base}.split(' ', Qt::SkipEmptyParts) + PowerShell::Snippets::to_bare_csv;

    QString ps_output;
    QString ps_output_err;
    if (PowerShell::exec(ps_args,
                         "Network Listing on Windows Platform",
                         &ps_output,
                         &ps_output_err))
    {
        std::map<std::string, mp::NetworkInterfaceInfo> ret{};
        for (const auto& line : ps_output.split(QRegularExpression{"[\r\n]"}, Qt::SkipEmptyParts))
        {
            auto terms = line.split(',', Qt::KeepEmptyParts);
            if (terms.size() != 4)
            {
                throw std::runtime_error{fmt::format(
                    "Could not determine available networks - unexpected powershell output: {}",
                    ps_output)};
            }

            auto iface = mp::NetworkInterfaceInfo{terms[0].toStdString(),
                                                  interpret_net_type(terms[1], terms[2]),
                                                  terms[3].toStdString()};
            ret.emplace(iface.id, iface);
        }

        return ret;
    }

    auto detail = ps_output_err.isEmpty() ? "" : fmt::format(" Detail: {}", ps_output_err);
    auto err = fmt::format(
        "Could not determine available networks - error executing powershell command.{}",
        detail);
    throw std::runtime_error{err};
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
            throw LesserWintermSyncException{"Could not find Windows Terminal's settings",
                                             profiles_path,
                                             "File not found"};

        auto json_root = read_winterm_settings(profiles_path);
        auto updated_json = update_profiles(profiles_path, json_root, winterm_setting);
        if (updated_json != json_root)
            save_profiles(profiles_path, updated_json);
    }
    catch (LesserWintermSyncException& e)
    {
        const auto level = winterm_setting == none ? mpl::Level::debug : mpl::Level::warning;
        mpl::log_message(level, log_category, e.what());
    }
    catch (ModerateWintermSyncException& e)
    {
        const auto level = winterm_setting == none ? mpl::Level::info : mpl::Level::error;
        mpl::log_message(level, log_category, e.what());
    }
    catch (GreaterWintermSyncException& e)
    {
        mpl::log_message(mpl::Level::error, log_category, e.what());
    }
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

std::string mp::platform::Platform::bridge_nomenclature() const
{
    return "switch";
}

bool mp::platform::Platform::can_reach_gateway(mp::IPAddress ip) const
{
    // ping
    throw mp::NotImplementedOnThisBackendException{"AZs @TODO"};
}

bool mp::platform::Platform::subnet_used_locally(mp::Subnet subnet) const
{
    // Get-NetAdapter | Get-NetIPAddress | Format-Table IPAddress,PrefixLength
    throw mp::NotImplementedOnThisBackendException{"AZs @TODO"};
}

QString mp::platform::Platform::daemon_config_home() const // temporary
{
    auto ret = systemprofile_app_data_path();
    ret = QDir{ret}.absoluteFilePath(
        "Local"); // what LOCALAPPDATA would point to under the system account, at this point
    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);

    if (QFile::exists(ret))
    {
        return ret; // should be something like
                    // "C:/Windows/system32/config/systemprofile/AppData/Local/multipassd"
    }
    else
    {
        return MP_PLATFORM.multipass_storage_location();
    }
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir,
                                                         AvailabilityZoneManager& az_manager)
{
    const auto driver = MP_SETTINGS.get(mp::driver_key);

    if (driver == QStringLiteral("hyperv"))
        return std::make_unique<HyperVVirtualMachineFactory>(data_dir, az_manager);
    else if (driver == QStringLiteral("virtualbox"))
    {
        qputenv("Path", qgetenv("Path") + ";C:\\Program Files\\Oracle\\VirtualBox"); /*
          This is where the Virtualbox installer puts things, and relying on PATH
          allows the user to do something about it, if the binaries are not found
          there.
        */

        return std::make_unique<VirtualBoxVirtualMachineFactory>(data_dir, az_manager);
    }

    throw std::runtime_error("Invalid virtualization driver set in the environment");
}

std::unique_ptr<mp::Process> mp::platform::make_sshfs_server_process(
    const mp::SSHFSServerConfig& config)
{
    return MP_PROCFACTORY.create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
}

std::unique_ptr<mp::Process> mp::platform::make_process(
    std::unique_ptr<mp::ProcessSpec>&& process_spec)
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
    logging::trace(kLogCategory,
                   "chown() called for `{}` (uid: {}, gid: {}) but it's no-op.",
                   path,
                   uid,
                   gid);
    return 0;
}

// new_ACL returns a new ACL unique ptr that retains all existing Hyper-V ACEs or NULL if no entries
// exist.
auto new_ACL(LPSTR path)
{
    auto deleter = [](ACL* acl) noexcept { return LocalFree(HLOCAL(acl)); };
    auto newACL = std::unique_ptr<ACL, decltype(deleter)>{NULL, deleter};

    PSECURITY_DESCRIPTOR pSD;
    auto pSD_guard = sg::make_scope_guard([&pSD]() noexcept { LocalFree((HLOCAL)(pSD)); });

    PACL pOldDACL = NULL;
    if (GetNamedSecurityInfo(path,
                             SE_FILE_OBJECT,
                             DACL_SECURITY_INFORMATION,
                             NULL,
                             NULL,
                             &pOldDACL,
                             NULL,
                             &pSD) != ERROR_SUCCESS)
    {
        return newACL;
    }

    DWORD size = sizeof(ACL);
    std::vector<ACCESS_ALLOWED_ACE*> aces{};

    // iterate over ACEs in old ACL.
    for (DWORD i = 0; i < pOldDACL->AceCount; ++i)
    {
        ACCESS_ALLOWED_ACE* pACE = NULL;
        if (!GetAce(pOldDACL, i, (LPVOID*)(&pACE)))
        {
            continue;
        }

        PSID sid = (PSID)(&pACE->SidStart);

        // convert SID to string so we can examine it.
        LPSTR sidStr = NULL;
        if (ConvertSidToStringSid(sid, &sidStr))
        {
            // Magic string given to us by
            // https://learn.microsoft.com/en-us/windows-server/identity/ad-ds/manage/understand-security-identifiers#well-known-sids
            // All Hyper-V SIDs begin with this magic string.
            static constexpr char VM_SID_ID[] = "S-1-5-83-";
            auto cmp = strncmp(sidStr, VM_SID_ID, sizeof(VM_SID_ID) - 1);
            LocalFree((HLOCAL)sidStr);

            if (cmp == 0)
            {
                aces.emplace_back(pACE);
                size += GetLengthSid(sid);
            }
        }
    }

    if (aces.size() > 0)
    {
        size += sizeof(ACCESS_ALLOWED_ACE) * aces.size();

        // Align size to a DWORD. (taken from
        // https://learn.microsoft.com/en-us/windows/win32/api/securitybaseapi/nf-securitybaseapi-initializeacl)
        size = (size + (sizeof(DWORD) - 1)) & 0xfffffffc;

        if (newACL.reset((PACL)LocalAlloc(LPTR, size)); newACL)
        {
            if (!InitializeAcl(newACL.get(), size, ACL_REVISION))
            {
                throw std::system_error(GetLastError(),
                                        std::system_category(),
                                        "Failed to initialize new ACL");
            }

            // try to add all Hyper-V ACEs
            for (const auto& ace : aces)
            {
                if (!AddAce(newACL.get(), ACL_REVISION, MAXDWORD, ace, ace->Header.AceSize))
                {
                    throw std::system_error(GetLastError(),
                                            std::system_category(),
                                            "Failed to add Hyper-V ACE to new ACL");
                }
            }
        }
    }

    return newACL;
}

bool mp::platform::Platform::set_permissions(const std::filesystem::path& path,
                                             std::filesystem::perms perms,
                                             bool inherit) const
{
    // Windows has both ACLs and very limited POSIX permissions

    // This handles the POSIX side of things.
    std::error_code ec{};
    std::filesystem::permissions(path, perms, ec);

    // Rest handles ACLs
    auto path_str = path.string();
    LPSTR lpPath = path_str.data();
    auto success = true;

    auto newACL = new_ACL(lpPath);

    // Wipe out current ACLs
    SetNamedSecurityInfo(lpPath,
                         SE_FILE_OBJECT,
                         DACL_SECURITY_INFORMATION,
                         nullptr,
                         nullptr,
                         newACL.get(),
                         nullptr);

    if (int others = int(perms) & 0007; others != 0)
        success &= set_specific_perms(lpPath, WinWorldSid, convert_permissions(others), inherit) ==
                   ERROR_SUCCESS;
    if (int group = int(perms) & 0070; group != 0)
        success &= set_specific_perms(lpPath,
                                      WinCreatorGroupSid,
                                      convert_permissions(group >> 3),
                                      inherit) == ERROR_SUCCESS;
    if (int owner = int(perms) & 0700; owner != 0)
        success &= set_specific_perms(lpPath,
                                      WinCreatorOwnerSid,
                                      convert_permissions(owner >> 6),
                                      inherit) == ERROR_SUCCESS;

    // #3216 Set the owner as Admin and give the Admins group blanket access
    success &= take_ownership(path);
    success &= set_specific_perms(lpPath, WinBuiltinAdministratorsSid, GENERIC_ALL, inherit) ==
               ERROR_SUCCESS;

    return success;
}

bool mp::platform::Platform::take_ownership(const std::filesystem::path& path) const
{
    auto path_str = path.string();
    LPSTR lpPath = path_str.data();

    return set_file_owner(lpPath, get_well_known_sid(WinBuiltinAdministratorsSid).get()) ==
           ERROR_SUCCESS;
}

void mp::platform::Platform::setup_permission_inheritance(bool) const
{
    // this does nothing since Windows doesn't use global state
}

bool mp::platform::Platform::symlink(const char* target, const char* link, bool is_dir) const
{
    DWORD flags =
        is_dir ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0x00 | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    return CreateSymbolicLink(link, target, flags);
}

bool mp::platform::Platform::link(const char* target, const char* link) const
{
    return CreateHardLink(link, target, nullptr);
}

int mp::platform::Platform::utime(const char* path, int atime, int mtime) const
{
    DWORD ret = NO_ERROR;
    auto handle = CreateFile(path,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_OPEN_REPARSE_POINT,
                             NULL);

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
    mp::PowerShell::exec(
        {"((Get-WMIObject -class Win32_ComputerSystem | Select-Object -ExpandProperty username))"},
        "get-username",
        &username);
    return username.section('\\', 1);
}

QDir mp::platform::Platform::get_alias_scripts_folder() const
{
    QDir aliases_folder;

    QString location = MP_STDPATHS.writableLocation(mp::StandardPaths::HomeLocation) +
                       "/AppData/local/multipass/bin";
    aliases_folder = QDir{location};

    if (!aliases_folder.mkpath(aliases_folder.path()))
        throw std::runtime_error(fmt::format("error creating \"{}\"\n", aliases_folder.path()));

    return aliases_folder;
}

void mp::platform::Platform::create_alias_script(const std::string& alias,
                                                 const mp::AliasDefinition& def) const
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
    ret.insert(
        std::make_unique<CustomSettingSpec>(winterm_key, petenv_default, [](const QString& val) {
            return interpret_setting(winterm_key, val);
        }));

    return ret;
}

std::string mp::platform::Platform::alias_path_message() const
{
    return fmt::format(
        "You'll need to add the script alias folder to your path for aliases to work\n"
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
    auto systemprofile_roaming_path =
        QDir{systemprofile_app_data_path()}.absoluteFilePath("Roaming");

    // If %PROGRAMDATA%\Multipass exists or if
    // %SYSTEMROOT%\system32\config\AppData\Roaming\multipassd doesn't exist, use
    // %PROGRAMDATA%\Multipass
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

std::function<std::optional<int>(const std::function<bool()>&)> mp::platform::make_quit_watchdog(
    const std::chrono::milliseconds& timeout)
{
    signal_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (signal_event == (HANDLE) nullptr)
        throw std::runtime_error("Unable to create signal object\n");

    SetConsoleCtrlHandler(signal_handler, TRUE);

    return
        [millis = timeout.count()](const std::function<bool()>& condition) -> std::optional<int> {
            ResetEvent(signal_event);

            while (condition())
            {
                // Ctrl+C will break this wait.
                if (WaitForSingleObject(signal_event, millis) != WAIT_TIMEOUT)
                    return 0;
            }

            return std::nullopt;
        };
}

std::string mp::platform::reinterpret_interface_id(const std::string& ux_id)
{
    auto ps_cmd =
        QStringLiteral(
            "Get-NetAdapter -Name \"%1\" | Select-Object -ExpandProperty InterfaceDescription")
            .arg(QString::fromStdString(ux_id))
            .split(' ', Qt::SkipEmptyParts);

    QString ps_output;
    QString ps_output_err;
    if (PowerShell::exec(ps_cmd, "Adapter description from name", &ps_output, &ps_output_err))
    {
        auto output_lines = ps_output.split(QRegularExpression{"[\r\n]"}, Qt::SkipEmptyParts);
        if (output_lines.size() != 1)
        {
            throw std::runtime_error{fmt::format("Could not obtain adapter description from name "
                                                 "\"{}\" - unexpected powershell output: {}",
                                                 ux_id,
                                                 ps_output)};
        }

        return output_lines.first().toStdString();
    }

    auto detail = ps_output_err.isEmpty() ? "" : fmt::format(" Detail: {}", ps_output_err);
    auto err = fmt::format("Could not obtain adapter description from name \"{}\" - error "
                           "executing powershell command.{}",
                           ux_id,
                           detail);
    throw std::runtime_error{err};
}

int mp::platform::Platform::get_cpus() const
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}

long long mp::platform::Platform::get_total_ram() const
{
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullTotalPhys;
}

std::filesystem::path mp::platform::Platform::get_root_cert_dir() const
{
    // FOLDERID_ProgramData returns C:\ProgramData normally
    const auto base_dir = get_wellknown_path(FOLDERID_ProgramData);

    // Windows doesn't use `daemon_name` for the data directory (see `program_data_multipass_path`)
    return base_dir / "Multipass" / "data";
}
