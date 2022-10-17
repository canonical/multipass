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
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/process/simple_process_spec.h>
#include <multipass/settings/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#ifdef QEMU_ENABLED
#include "backends/qemu/qemu_virtual_machine_factory.h"
#else
#define QEMU_ENABLED 0
#endif

#ifdef VIRTUALBOX_ENABLED
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#else
#define VIRTUALBOX_ENABLED 0
#endif

#include "platform_proprietary.h"
#include "platform_shared.h"
#include "shared/macos/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <daemon/default_vm_image_vault.h>
#include <default_update_prompt.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QOperatingSystemVersion>
#include <QRegularExpression>
#include <QString>
#include <QtGlobal>

#include <cassert>
#include <utility>
#include <vector>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mu = multipass::utils;

namespace
{
constexpr auto application_id = "com.canonical.multipass";
constexpr auto autostart_filename = "com.canonical.multipass.gui.autostart.plist";
constexpr auto autostart_link_subdir = "Library/LaunchAgents";
constexpr auto category = "osx platform";

QString interpret_macos_hotkey(QString val)
{
    static const auto key_mapping = std::vector<std::pair<std::vector<QString>, QString>>{
        {{"meta", "option", "opt"}, "alt"}, // qt does not understand "opt"
        {{"ctrl", "control"}, "meta"},      // qt takes "meta" to mean ctrl
        {{"cmd", "command"}, "ctrl"},       // qt takes "ctrl" to mean cmd
    };                                      // Notice the order matters!

    for (const auto& [before_keys, after_key] : key_mapping)
        for (const auto& before_key : before_keys)
            val.replace(before_key, after_key, Qt::CaseInsensitive);

    return mp::platform::interpret_hotkey(val);
}

QString get_networksetup_output()
{
    auto nsetup_spec = mp::simple_process_spec("networksetup", {"-listallhardwareports"});
    auto nsetup_process = mp::platform::make_process(std::move(nsetup_spec));
    auto nsetup_exit_state = nsetup_process->execute();

    if (!nsetup_exit_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("networksetup failed ({}) with the following output:\n",
                                             nsetup_exit_state.failure_message(),
                                             nsetup_process->read_all_standard_error()));
    }

    return nsetup_process->read_all_standard_output();
}

QString get_ifconfig_output()
{
    auto ifconfig_spec = mp::simple_process_spec("ifconfig", {});
    auto ifconfig_process = mp::platform::make_process(std::move(ifconfig_spec));
    auto ifconfig_exit_state = ifconfig_process->execute();

    if (!ifconfig_exit_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("ifconfig failed ({}) with the following output:\n{}",
                                             ifconfig_exit_state.failure_message(),
                                             ifconfig_process->read_all_standard_error()));
    }

    return ifconfig_process->read_all_standard_output();
}

QStringList get_bridged_interfaces(const QString& if_name, const QString& ifconfig_output)
{
    // Search the substring of the full ifconfig output containing only the interface if_name.
    int start = ifconfig_output.indexOf(
        QRegularExpression{QStringLiteral("^%1:").arg(if_name), QRegularExpression::MultilineOption});
    int end = ifconfig_output.indexOf(QRegularExpression("^\\w+:", QRegularExpression::MultilineOption), start + 1);
    QStringRef ifconfig_entry = ifconfig_output.midRef(start, end - start);

    // Search for the bridged interfaces in the resulting string ref.
    const auto pattern = QStringLiteral("^[ \\t]+member: (?<member>\\w+) flags.*$");
    const auto regexp = QRegularExpression{pattern, QRegularExpression::MultilineOption};
    QRegularExpressionMatchIterator match_it = regexp.globalMatch(ifconfig_entry);

    QStringList bridged_ifs;

    while (match_it.hasNext())
    {
        auto match = match_it.next();
        if (match.hasMatch())
        {
            bridged_ifs.push_back(match.captured("member"));
        }
    }

    return bridged_ifs;
}

std::string describe_bridge(const QString& name, const QString& ifconfig_output)
{
    auto members = get_bridged_interfaces(name, ifconfig_output);
    return members.isEmpty() ? "Empty network bridge" : fmt::format("Network bridge with {}", members.join(", "));
}

std::optional<mp::NetworkInterfaceInfo> get_net_info(const QString& nsetup_entry, const QString& ifconfig_output)
{
    const auto name_pattern = QStringLiteral("^Device: ([\\w -]+)$");
    const auto desc_pattern = QStringLiteral("^Hardware Port: (.+)$");
    const auto name_regex = QRegularExpression{name_pattern, QRegularExpression::MultilineOption};
    const auto desc_regex = QRegularExpression{desc_pattern, QRegularExpression::MultilineOption};
    const auto name = name_regex.match(nsetup_entry).captured(1);
    const auto desc = desc_regex.match(nsetup_entry).captured(1);

    mpl::log(mpl::Level::trace, category, fmt::format("Parsing networksetup chunk:\n{}", nsetup_entry));

    if (!name.isEmpty() && !desc.isEmpty())
    {
        auto id = name.toStdString();

        // bridges first, so we match on things like "thunderbolt bridge" here
        if (name.contains("bridge") || desc.contains("bridge", Qt::CaseInsensitive))
            return mp::NetworkInterfaceInfo{id, "bridge", describe_bridge(name, ifconfig_output)};

        // simple cases next
        auto description = desc.toStdString();
        for (const auto& type : {"thunderbolt", "ethernet", "usb"})
            if (desc.contains(type, Qt::CaseInsensitive))
                return mp::NetworkInterfaceInfo{id, type, description};

        // finally wifi, which we report without a dash in the middle
        if (desc.contains("wi-fi", Qt::CaseInsensitive))
            return mp::NetworkInterfaceInfo{id, "wifi", description};

        mpl::log(mpl::Level::warning, category, fmt::format("Unsupported device \"{}\" ({})", id, description));
    }

    mpl::log(mpl::Level::trace, category, fmt::format("Skipping chunk"));
    return std::nullopt;
}

std::string get_alias_script_path(const std::string& alias)
{
    QDir aliases_folder = MP_PLATFORM.get_alias_scripts_folder();

    return aliases_folder.absoluteFilePath(QString::fromStdString(alias)).toStdString();
}
} // namespace

std::map<std::string, mp::NetworkInterfaceInfo> mp::platform::Platform::get_network_interfaces_info() const
{
    auto networks = std::map<std::string, mp::NetworkInterfaceInfo>();

    // Get the output of 'ifconfig'.
    auto ifconfig_output = get_ifconfig_output();
    auto nsetup_output = get_networksetup_output();

    mpl::log(mpl::Level::trace, category, fmt::format("Got the following output from ifconfig:\n{}", ifconfig_output));
    mpl::log(mpl::Level::trace, category,
             fmt::format("Got the following output from networksetup:\n{}", nsetup_output));

    // split the output of networksetup in multiple entries (one per interface)
    auto empty_line_regex = QRegularExpression(QStringLiteral("^$"), QRegularExpression::MultilineOption);
    auto nsetup_entries = nsetup_output.split(empty_line_regex, Qt::SkipEmptyParts);

    // Parse the output we got to obtain each interface's properties
    for (const auto& nsetup_entry : nsetup_entries)
    {
        if (auto net_info = get_net_info(nsetup_entry, ifconfig_output); net_info)
        {
            const auto& net_id = net_info->id;
            networks.emplace(net_id, std::move(*net_info));
        }
    }

    return networks;
}

bool mp::platform::Platform::is_alias_supported(const std::string& alias, const std::string& remote) const
{
    auto driver = MP_SETTINGS.get(mp::driver_key);

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
    return (backend == "qemu" && QEMU_ENABLED &&
            QOperatingSystemVersion::current() >= QOperatingSystemVersion::MacOSCatalina) ||
           (backend == "virtualbox" && VIRTUALBOX_ENABLED);
}

auto mp::platform::Platform::extra_daemon_settings() const -> SettingSpec::Set
{
    return {};
}

auto mp::platform::Platform::extra_client_settings() const -> SettingSpec::Set
{
    return {};
}

QString mp::platform::interpret_setting(const QString& key, const QString& val)
{
    if (key == hotkey_key)
        return interpret_macos_hotkey(val);

    // this should not happen (settings should have found it to be an invalid key)
    throw InvalidSettingException(key, val, "Setting unavailable on macOS");
}

void mp::platform::sync_winterm_profiles()
{
    // NOOP on macOS
}

QString mp::platform::autostart_test_data()
{
    return autostart_filename;
}

void mp::platform::setup_gui_autostart_prerequisites()
{
    const auto link_dir = QDir{QDir::home().absoluteFilePath(autostart_link_subdir)};
    const auto autostart_subdir = QDir{application_id}.filePath("Resources");
    mu::link_autostart_file(link_dir, autostart_subdir, autostart_filename);
}

std::string mp::platform::default_server_address()
{
    return {"unix:/var/run/multipass_socket"};
}

QString mp::platform::Platform::default_driver() const
{
    assert(QEMU_ENABLED);
    return QStringLiteral("qemu");
}

QString mp::platform::Platform::default_privileged_mounts() const
{
    return QStringLiteral("true");
}

bool mp::platform::Platform::is_image_url_supported() const
{
    const auto driver = MP_SETTINGS.get(mp::driver_key);

    if (driver == "virtualbox" || driver == "qemu")
        return check_unlock_code();

    return false;
}

QString mp::platform::Platform::daemon_config_home() const // temporary
{
    auto ret = QStringLiteral("/var/root/Library/Preferences/");
    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);

    return ret;
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    auto driver = MP_SETTINGS.get(mp::driver_key);

    if (driver == QStringLiteral("virtualbox"))
    {
#if VIRTUALBOX_ENABLED
        qputenv("PATH", qgetenv("PATH") + ":/usr/local/bin"); /*
          This is where the Virtualbox installer puts things, and relying on PATH
          allows the user to do something about it, if the binaries are not found
          there.
        */

        return std::make_unique<VirtualBoxVirtualMachineFactory>();
#endif
    }
    else if (driver == QStringLiteral("qemu"))
    {
#if QEMU_ENABLED
        return std::make_unique<QemuVirtualMachineFactory>(data_dir);
#endif
    }

    throw std::runtime_error(fmt::format("Unsupported virtualization driver: {}", driver));
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
    return nullptr;
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DefaultUpdatePrompt>();
}

bool mp::platform::Platform::link(const char* target, const char* link) const
{
    QFileInfo file_info{target};

    if (file_info.isSymLink())
    {
        return ::symlink(file_info.symLinkTarget().toStdString().c_str(), link) == 0;
    }

    return ::link(target, link) == 0;
}

QDir mp::platform::Platform::get_alias_scripts_folder() const
{
    QDir aliases_folder;

    QString location = MP_STDPATHS.writableLocation(mp::StandardPaths::AppLocalDataLocation) + "/bin";
    aliases_folder = QDir{location};

    return aliases_folder;
}

void mp::platform::Platform::create_alias_script(const std::string& alias, const mp::AliasDefinition& def) const
{
    auto file_path = get_alias_script_path(alias);

    auto multipass_exec = QCoreApplication::applicationFilePath().toStdString();

    std::string script = "#!/bin/sh\n\n\"" + multipass_exec + "\" " + alias + " -- " + "\"${@}\"\n";

    MP_UTILS.make_file_with_content(file_path, script, true);

    QFile file(QString::fromStdString(file_path));
    auto permissions =
        MP_FILEOPS.permissions(file) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;

    if (!MP_FILEOPS.setPermissions(file, permissions))
        throw std::runtime_error(fmt::format("cannot set permissions to alias script '{}'", file_path));
}

void mp::platform::Platform::remove_alias_script(const std::string& alias) const
{
    std::string file_path = get_alias_script_path(alias);

    if (::unlink(file_path.c_str()))
        throw std::runtime_error(strerror(errno));
}

std::string mp::platform::reinterpret_interface_id(const std::string& ux_id)
{
    return ux_id;
}
