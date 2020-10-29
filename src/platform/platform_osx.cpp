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
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/hyperkit/hyperkit_virtual_machine_factory.h"
#include "backends/virtualbox/virtualbox_virtual_machine_factory.h"
#include "platform_proprietary.h"
#include "platform_shared.h"
#include "shared/macos/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <daemon/default_vm_image_vault.h>
#include <default_update_prompt.h>

#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QString>
#include <QtGlobal>

#include <utility>
#include <vector>

#include <unistd.h>

namespace mp = multipass;
namespace mu = multipass::utils;

namespace
{
constexpr auto application_id = "com.canonical.multipass";
constexpr auto autostart_filename = "com.canonical.multipass.gui.autostart.plist";
constexpr auto autostart_link_subdir = "Library/LaunchAgents";

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

} // namespace

std::map<QString, QString> mp::platform::extra_settings_defaults()
{
    return {};
}

QString mp::platform::interpret_setting(const QString& key, const QString& val)
{
    if (key == hotkey_key)
        return interpret_macos_hotkey(val);

    // this should not happen (settings should have found it to be an invalid key)
    throw InvalidSettingsException(key, val, "Setting unavailable on macOS");
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

QString mp::platform::default_driver()
{
    return QStringLiteral("hyperkit");
}

QString mp::platform::daemon_config_home() // temporary
{
    auto ret = QStringLiteral("/var/root/Library/Preferences/");
    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);

    return ret;
}

bool mp::platform::is_backend_supported(const QString& backend)
{
    return backend == "hyperkit" || backend == "virtualbox";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    auto driver = utils::get_driver_str();

    if (driver == QStringLiteral("hyperkit"))
        return std::make_unique<HyperkitVirtualMachineFactory>();
    else if (driver == QStringLiteral("virtualbox"))
    {
        qputenv("PATH", qgetenv("PATH") + ":/usr/local/bin"); /*
          This is where the Virtualbox installer puts things, and relying on PATH
          allows the user to do something about it, if the binaries are not found
          there.
        */

        return std::make_unique<VirtualBoxVirtualMachineFactory>();
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

bool mp::platform::link(const char* target, const char* link)
{
    QFileInfo file_info{target};

    if (file_info.isSymLink())
    {
        return ::symlink(file_info.symLinkTarget().toStdString().c_str(), link) == 0;
    }

    return ::link(target, link) == 0;
}

bool mp::platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    auto driver = utils::get_driver_str();

    // Minimal image that snapcraft:core is does not work with VirtualBox
    if (driver == "virtualbox" && remote == "snapcraft" && alias == "core")
        return false;

    // Core images don't work on hyperkit yet
    if (driver == "hyperkit" && remote.empty() && (supported_core_aliases.find(alias) != supported_core_aliases.end()))
        return false;

    // Core-based appliance images don't work on hyperkit yet
    if (driver == "hyperkit" && remote == "appliance")
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
            if (it->second.empty() || (it->second.find(alias) != it->second.end()))
                return true;
        }
    }

    return false;
}

bool mp::platform::is_remote_supported(const std::string& remote)
{
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
    auto driver = utils::get_driver_str();

    if (driver == "virtualbox")
        return check_unlock_code();

    return false;
}

