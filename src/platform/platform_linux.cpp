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
#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/settings/settings.h>
#include <multipass/snap_utils.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/lxd/lxd_virtual_machine_factory.h"

#ifdef QEMU_ENABLED
#include "backends/qemu/qemu_virtual_machine_factory.h"
#endif

#ifdef MULTIPASS_JOURNALD_ENABLED
#include "logger/journald_logger.h"
#else
#include "logger/syslog_logger.h"
#endif

#include "platform_linux_detail.h"
#include "platform_shared.h"
#include "shared/linux/process_factory.h"
#include "shared/sshfs_server_process_spec.h"
#include <disabled_update_prompt.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>

#include <errno.h>
#include <linux/if_arp.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "Linux platform";
constexpr auto br_nomenclature = "bridge";

// Fetch the ARP protocol HARDWARE identifier.
int get_net_type(const QDir& net_dir) // types defined in if_arp.h
{
    static constexpr auto default_ret = -1;
    static const auto type_filename = QStringLiteral("type");

    QFile type_file{net_dir.filePath(type_filename)};
    if (type_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        bool ok;
        auto got = QTextStream{&type_file}.read(6).toInt(&ok); // 6 chars enough for up to 0xFFFF; 0 returned on failure
        return ok ? got : default_ret;
    }

    auto snap_hint = mpu::in_multipass_snap() ? " Is the 'network-observe' snap interface connected?" : "";
    mpl::log(mpl::Level::warning, category, fmt::format("Could not read {}.{}", type_file.fileName(), snap_hint));

    return default_ret;
}

// device types found in Linux source (in drivers/net/): PHY, bareudp, bond, geneve, gtp, macsec, ppp, vxlan, wlan, wwan
// should be empty for ethernet
QString get_net_devtype(const QDir& net_dir)
{
    static constexpr auto max_read = 5000;
    static const auto uevent_filename = QStringLiteral("uevent");
    static const auto devtype_regex =
        QRegularExpression{QStringLiteral("^DEVTYPE=(.*)$"), QRegularExpression::MultilineOption};

    QFile uevent_file{net_dir.filePath(uevent_filename)};
    if (uevent_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        auto contents = QTextStream{&uevent_file}.read(max_read);
        return devtype_regex.match(contents).captured(1);
    }

    mpl::log(mpl::Level::warning, category, fmt::format("Could not read {}", uevent_file.fileName()));
    return {};
}

bool is_virtual_net(const QDir& net_dir)
{
    static const auto virtual_dir = QStringLiteral("virtual");

    return net_dir.canonicalPath().contains(virtual_dir, Qt::CaseInsensitive);
}

bool is_ethernet(const QDir& net_dir)
{
    static const auto wireless = QStringLiteral("wireless");

    return !is_virtual_net(net_dir) && !net_dir.exists(wireless) && get_net_type(net_dir) == ARPHRD_ETHER &&
           get_net_devtype(net_dir).isEmpty();
}

std::optional<mp::NetworkInterfaceInfo> get_network(const QDir& net_dir)
{
    static const auto bridge_fname = QStringLiteral("brif");
    auto id = net_dir.dirName().toStdString();

    if (net_dir.exists(br_nomenclature))
    {
        std::vector<std::string> links;
        QStringList bridge_members = QDir{net_dir.filePath(bridge_fname)}.entryList(QDir::NoDotAndDotDot | QDir::Dirs);

        links.reserve(bridge_members.size());
        std::transform(bridge_members.cbegin(), bridge_members.cend(), std::back_inserter(links),
                       [](const QString& interface) { return interface.toStdString(); });

        return {{std::move(id),
                 br_nomenclature,
                 /*description=*/"",
                 std::move(links)}}; // description needs updating with links
    }
    else if (is_ethernet(net_dir))
        return {{std::move(id), "ethernet", "Ethernet device"}};

    return std::nullopt;
}

void update_bridges(std::map<std::string, mp::NetworkInterfaceInfo>& networks)
{
    for (auto& item : networks)
    {
        if (auto& net = item.second; net.type == br_nomenclature)
        { // bridge descriptions and links depend on what other networks we recognized
            auto& links = net.links;
            auto is_unknown = [&networks](const std::string& id) {
                auto same_as = [&id](const auto& other) { return other.first == id; };
                return std::find_if(networks.cbegin(), networks.cend(), same_as) == networks.cend();
            };
            links.erase(std::remove_if(links.begin(), links.end(), is_unknown),
                        links.end()); // filter links to networks we don't recognize

            net.description =
                links.empty() ? "Network bridge"
                              : fmt::format("Network bridge with {}", fmt::join(links.cbegin(), links.cend(), ", "));
        }
    }
}

std::string get_alias_script_path(const std::string& alias)
{
    auto aliases_folder = MP_PLATFORM.get_alias_scripts_folder();

    return aliases_folder.absoluteFilePath(QString::fromStdString(alias)).toStdString();
}
} // namespace

std::unique_ptr<QFile> multipass::platform::detail::find_os_release()
{
    const std::array<QString, 3> options{QStringLiteral("/var/lib/snapd/hostfs/etc/os-release"),
                                         QStringLiteral("/var/lib/snapd/hostfs/usr/lib/os-release"),
                                         QStringLiteral("")};

    auto it = options.cbegin();
    auto ret = std::make_unique<QFile>(*it);
    while (++it != options.cend() && !MP_FILEOPS.open(*ret, QIODevice::ReadOnly | QIODevice::Text))
        ret->setFileName(*it);

    return ret;
}

std::pair<QString, QString> multipass::platform::detail::parse_os_release(const QStringList& os_data)
{
    const QString id_field = "NAME";
    const QString version_field = "VERSION_ID";

    QString distro_id = "unknown";
    QString distro_rel = "unknown";

    auto strip_quotes = [](const QString& val) -> QString { return val.mid(1, val.length() - 2); };

    for (const QString& line : os_data)
    {
        QStringList split = line.split('=', Qt::KeepEmptyParts);
        if (split.length() == 2 && split[1].length() > 2) // Check for at least 1 char between quotes.
        {
            if (split[0] == id_field)
                distro_id = strip_quotes(split[1]);
            else if (split[0] == version_field)
                distro_rel = strip_quotes(split[1]);
        }
    }

    return std::pair(distro_id, distro_rel);
}

std::string multipass::platform::detail::read_os_release()
{
    std::unique_ptr<QFile> os_release = find_os_release();

    QStringList os_info;
    if (MP_FILEOPS.is_open(*os_release))
    {
        QTextStream input(os_release.get());
        QString line = MP_FILEOPS.read_line(input);
        while (!line.isNull())
        {
            os_info.append(line);
            line = MP_FILEOPS.read_line(input);
        }
        os_release->close();

        auto parsed = parse_os_release(os_info);

        return fmt::format("{}-{}", parsed.first, parsed.second);
    }

    return "unknown-unknown";
}

std::map<std::string, mp::NetworkInterfaceInfo> mp::platform::Platform::get_network_interfaces_info() const
{
    static const auto sysfs = QDir{QStringLiteral("/sys/class/net")};
    return detail::get_network_interfaces_from(sysfs);
}

QString mp::platform::Platform::get_blueprints_url_override() const
{
    return QString::fromUtf8(qgetenv("MULTIPASS_BLUEPRINTS_URL"));
}

bool mp::platform::Platform::is_alias_supported(const std::string& alias, const std::string& remote) const
{
    if (remote == mp::snapcraft_remote)
    {
        return supported_snapcraft_aliases.find(alias) != supported_snapcraft_aliases.end();
    }

    return true;
}

bool mp::platform::Platform::is_remote_supported(const std::string& remote) const
{
    return true;
}

bool mp::platform::Platform::is_backend_supported(const QString& backend) const
{
    return
#ifdef QEMU_ENABLED
        backend == "qemu" ||
#endif
        backend == "libvirt" || backend == "lxd";
}

bool mp::platform::Platform::link(const char* target, const char* link) const
{
    return ::link(target, link) == 0;
}

QDir mp::platform::Platform::get_alias_scripts_folder() const
{
    QDir aliases_folder;

    if (mpu::in_multipass_snap())
    {
        aliases_folder = QDir(QString(mpu::snap_user_common_dir()) + "/bin");
    }
    else
    {
        QString location = MP_STDPATHS.writableLocation(mp::StandardPaths::AppLocalDataLocation) + "/bin";
        aliases_folder = QDir{location};
    }

    return aliases_folder;
}

void mp::platform::Platform::create_alias_script(const std::string& alias, const mp::AliasDefinition& def) const
{
    std::string file_path = get_alias_script_path(alias);

    std::string multipass_exec = mpu::in_multipass_snap()
                                     ? "exec /usr/bin/snap run multipass"
                                     : fmt::format("{:?}", QCoreApplication::applicationFilePath().toStdString());

    std::string script = "#!/bin/sh\n\n" + multipass_exec + " " + alias + " -- \"${@}\"\n";

    MP_UTILS.make_file_with_content(file_path, script, true);

    QFile file(QString::fromStdString(file_path));
    auto permissions =
        MP_FILEOPS.permissions(file) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther;

    if (!MP_FILEOPS.setPermissions(file, permissions))
        throw std::runtime_error(fmt::format("cannot set permissions to alias script '{}'", file_path));
}

void mp::platform::Platform::remove_alias_script(const std::string& alias) const
{
    auto file_path = get_alias_script_path(alias);

    if (::unlink(file_path.c_str()))
        throw std::runtime_error(strerror(errno));
}

auto mp::platform::Platform::extra_daemon_settings() const -> SettingSpec::Set
{
    return {};
}

auto mp::platform::Platform::extra_client_settings() const -> SettingSpec::Set
{
    return {};
}

QString mp::platform::Platform::daemon_config_home() const // temporary
{
    auto ret = QString{qgetenv("DAEMON_CONFIG_HOME")};
    if (ret.isEmpty())
        ret = QStringLiteral("/root/.config");

    ret = QDir{ret}.absoluteFilePath(mp::daemon_name);
    return ret;
}

QString mp::platform::Platform::default_driver() const
{
    return QStringLiteral(
#ifdef QEMU_ENABLED
        "qemu"
#else
        "lxd"
#endif
    );
}

QString mp::platform::Platform::default_privileged_mounts() const
{
    return QStringLiteral("true");
}

bool mp::platform::Platform::is_image_url_supported() const
{
    return true;
}

std::string mp::platform::Platform::bridge_nomenclature() const
{
    return br_nomenclature;
}

auto mp::platform::detail::get_network_interfaces_from(const QDir& sys_dir)
    -> std::map<std::string, NetworkInterfaceInfo>
{
    auto ifaces_info = std::map<std::string, mp::NetworkInterfaceInfo>();
    for (const auto& entry : sys_dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs))
    {
        if (auto iface = get_network(QDir{sys_dir.filePath(entry)}); iface)
        {
            auto name = iface->id; // (can't rely on param evaluation order)
            ifaces_info.emplace(std::move(name), std::move(*iface));
        }
    }

    update_bridges(ifaces_info);

    return ifaces_info;
}

QString mp::platform::interpret_setting(const QString& key, const QString& val)
{
    // this should not happen (settings should have found it to be an invalid key)
    throw InvalidSettingException(key, val, "Setting unavailable on Linux");
}

void mp::platform::sync_winterm_profiles()
{
    // NOOP on Linux
}

std::string mp::platform::default_server_address()
{
    std::string base_dir;

    try
    {
        // if Snap, client and daemon can both access $SNAP_COMMON so can put socket there
        base_dir = mpu::snap_common_dir().toStdString();
    }
    catch (const mp::SnapEnvironmentException&)
    {
        base_dir = "/run";
    }
    return "unix:" + base_dir + "/multipass_socket";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    const auto& driver = MP_SETTINGS.get(mp::driver_key);
#ifdef QEMU_ENABLED
    if (driver == QStringLiteral("qemu"))
        return std::make_unique<QemuVirtualMachineFactory>(data_dir);
#endif

    if (driver == QStringLiteral("libvirt"))
        return std::make_unique<LibVirtVirtualMachineFactory>(data_dir);

    if (driver == QStringLiteral("lxd"))
        return std::make_unique<LXDVirtualMachineFactory>(data_dir);

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
#if MULTIPASS_JOURNALD_ENABLED
    return std::make_unique<logging::JournaldLogger>(level);
#else
    return std::make_unique<logging::SyslogLogger>(level);
#endif
}

std::string mp::platform::reinterpret_interface_id(const std::string& ux_id)
{
    return ux_id;
}

std::string multipass::platform::host_version()
{
    return mpu::in_multipass_snap() ? multipass::platform::detail::read_os_release()
                                    : fmt::format("{}-{}", QSysInfo::productType(), QSysInfo::productVersion());
}
