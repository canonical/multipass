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
#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/snap_utils.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/lxd/lxd_virtual_machine_factory.h"
#include "backends/qemu/qemu_virtual_machine_factory.h"
#include "logger/journald_logger.h"
#include "platform_shared.h"
#include "shared/linux/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <disabled_update_prompt.h>

#include <arpa/inet.h>
#include <cstring>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QRegularExpression>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mu = multipass::utils;

namespace
{
constexpr auto autostart_filename = "multipass.gui.autostart.desktop";

} // namespace

std::map<QString, QString> mp::platform::extra_settings_defaults()
{
    return {};
}

QString mp::platform::interpret_setting(const QString& key, const QString& val)
{
    if (key == hotkey_key)
        return mp::platform::interpret_hotkey(val);

    // this should not happen (settings should have found it to be an invalid key)
    throw InvalidSettingsException(key, val, "Setting unavailable on Linux");
}

void mp::platform::sync_winterm_profiles()
{
    // NOOP on Linux
}

QString mp::platform::autostart_test_data()
{
    return autostart_filename;
}

void mp::platform::setup_gui_autostart_prerequisites()
{
    const auto config_dir = QDir{MP_STDPATHS.writableLocation(StandardPaths::GenericConfigLocation)};
    const auto link_dir = QDir{config_dir.absoluteFilePath("autostart")};
    mu::link_autostart_file(link_dir, mp::client_name, autostart_filename);
}

std::string mp::platform::default_server_address()
{
    std::string base_dir;

    try
    {
        // if Snap, client and daemon can both access $SNAP_COMMON so can put socket there
        base_dir = mu::snap_common_dir().toStdString();
    }
    catch (const mp::SnapEnvironmentException&)
    {
        base_dir = "/run";
    }
    return "unix:" + base_dir + "/multipass_socket";
}

QString mp::platform::default_driver()
{
    return QStringLiteral("qemu");
}

QString mp::platform::daemon_config_home() // temporary
{
    auto ret = QString{qgetenv("DAEMON_CONFIG_HOME")};
    if (ret.isEmpty())
        ret = QStringLiteral("/root/.config");

    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);
    return ret;
}

bool mp::platform::is_backend_supported(const QString& backend)
{
    return backend == "qemu" || backend == "libvirt" || backend == "lxd";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    const auto& driver = utils::get_driver_str();
    if (driver == QStringLiteral("qemu"))
        return std::make_unique<QemuVirtualMachineFactory>(data_dir);
    else if (driver == QStringLiteral("libvirt"))
        return std::make_unique<LibVirtVirtualMachineFactory>(data_dir);
    else if (driver == QStringLiteral("lxd"))
        return std::make_unique<LXDVirtualMachineFactory>(data_dir);
    else
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

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DisabledUpdatePrompt>();
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::JournaldLogger>(level);
}

bool mp::platform::link(const char* target, const char* link)
{
    return ::link(target, link) == 0;
}

bool mp::platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    return true;
}

bool mp::platform::is_remote_supported(const std::string& remote)
{
    return true;
}

bool mp::platform::is_image_url_supported()
{
    return true;
}

mp::IPAddress get_ip_address(const std::string& iface_name)
{
    const char* iface = iface_name.c_str();
    struct ifreq ifr;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    ioctl(fd, SIOCGIFADDR, &ifr);

    close(fd);

    return mp::IPAddress(inet_ntoa((reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr))->sin_addr));
}

// The uevent files in /sys have a set of KEY=value pairs, one per line. This function takes as arguments a file
// with full path and a key, and returns the value associated with that key. If the specified uevent file does not
// exist, or the key is not found, then the empty string is returned.
std::string get_uevent_value(const std::string& file, const std::string& key)
{
    QRegularExpression pattern('^' + QString::fromStdString(key) + "=(?<value>[A-Za-z0-9-_]*)$");
    QFile uevent_file(QString::fromStdString(file));

    if (uevent_file.open(QIODevice::ReadOnly))
    {
        while (!uevent_file.atEnd())
        {
            auto key_match = pattern.match(uevent_file.readLine());
            if (key_match.hasMatch())
            {
                uevent_file.close();
                return key_match.captured("value").toStdString();
            }
        }
        uevent_file.close();
    }

    return std::string();
}

// TODO: refactor this long function.
mp::NetworkInterfaceInfo mp::platform::get_network_interface_info(const std::string& iface_name)
{
    QString iface_name_qstr = QString::fromStdString(iface_name);
    std::string type;
    std::string description;
    mp::optional<mp::IPAddress> ip_address;

    QString iface_dir_name("/sys/class/net/" + iface_name_qstr);

    // The carrier file only says if the interface is up. If it is, we get the IP.
    QFile carrier_file(iface_dir_name + "/carrier");
    if (carrier_file.open(QIODevice::ReadOnly))
    {
        auto carrier_line = carrier_file.readLine();
        carrier_file.close();
        if (carrier_line[0] == '1')
        {
            ip_address = get_ip_address(iface_name);
        }
    }

    // Distinguish between hardware and virtual interfaces. In the virtual/ folder, there is one entry for each
    // virtual interface.
    if (QFile::exists("/sys/devices/virtual/net/" + iface_name_qstr))
    {
        // TUN and TAP devices have a file containing flags. The only way of knowing which one of both types the
        // interface is, is to parse the flags.
        QFile tun_flags_file(iface_dir_name + "/tun_flags");
        if (tun_flags_file.open(QIODevice::ReadOnly))
        {
            long tun_flags = tun_flags_file.readLine().trimmed().toLong(nullptr, 0);
            tun_flags_file.close();
            if (tun_flags & IFF_TUN)
            {
                description = "TUN ";
            }
            else
            {
                if (tun_flags & IFF_TAP)
                {
                    description = "TAP ";
                }
            }
        }

        // A virtual interface can be bridge or bridged, but not both.
        bool is_bridge = QFile::exists(iface_dir_name + "/bridge");
        if (is_bridge)
        {
            type = "bridge";
            QDir bridged_dir(iface_dir_name + "/brif");
            QStringList all_bridged = bridged_dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
            if (all_bridged.isEmpty())
            {
                description += "Empty network bridge";
            }
            else
            {
                description += "Network bridge containing " + all_bridged.join(", ").toStdString();
            }
        }
        else
        {
            std::string associated_to =
                get_uevent_value((iface_dir_name + "/brport/bridge/uevent").toStdString(), "INTERFACE");
            type = "virtual";
            if (associated_to.empty())
            {
                description += "Virtual interface";
            }
            else
            {
                description += "Virtual interface associated to " + associated_to;
            }
        }
    }
    else
    {
        type = QFile::exists(iface_dir_name + "/wireless") ? "wifi" : "ethernet";

        QFile vendor_file(iface_dir_name + "/device/vendor");
        std::string vendor;
        if (vendor_file.open(QIODevice::ReadOnly))
        {
            vendor = vendor_file.readLine().mid(2, 4).toStdString();
            vendor_file.close();

            QFile model_file(iface_dir_name + "/device/device");
            std::string model;
            if (model_file.open(QIODevice::ReadOnly))
            {
                model = model_file.readLine().mid(2, 4).toStdString();
                model_file.close();
            }
            description = vendor + ":" + model;
        }
        else
        {
            // If there is no vendor specified, this can be a USB interface.
            description =
                get_uevent_value((iface_dir_name + "/device/uevent").toStdString(), "DEVTYPE") == "usb_interface"
                    ? "USB interface"
                    : "Unknown";
        }
    }

    mp::NetworkInterfaceInfo iface_info{iface_name, type, description, ip_address};

    return iface_info;
}

std::map<std::string, mp::NetworkInterfaceInfo> mp::platform::get_network_interfaces_info()
{
    auto ifaces_info = std::map<std::string, mp::NetworkInterfaceInfo>();
    QDir iface_dir("/sys/class/net");
    QFileInfoList all_files_in_dir = iface_dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs);

    for (auto entry : all_files_in_dir)
    {
        auto entry_str = entry.baseName().toStdString();
        ifaces_info.emplace(std::make_pair(entry_str, get_network_interface_info(entry_str)));
    }

    return ifaces_info;
}
