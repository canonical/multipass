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
#include "shared/macos/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <default_update_prompt.h>

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QtGlobal>

#include <unistd.h>

namespace mp = multipass;
namespace mu = multipass::utils;

namespace
{
constexpr auto application_id = "com.canonical.multipass";
constexpr auto autostart_filename = "com.canonical.multipass.gui.autostart.plist";
constexpr auto autostart_link_subdir = "Library/LaunchAgents";

} // namespace

std::map<QString, QString> mp::platform::extra_settings_defaults()
{
    return {};
}

QString mp::platform::interpret_winterm_integration(const QString& val)
{
    // this should not happen (settings would have found it to be an invalid key)
    throw InvalidSettingsException{winterm_key, val, "Windows Terminal is not available on macOS"};
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
    return mp::ProcessFactory::instance().create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
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

    // Minimal images that the snapcraft remote uses do not work with VirtualBox
    if (driver == "virtualbox" && remote == "snapcraft")
        return false;

    if (remote.empty() && alias == "core")
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

