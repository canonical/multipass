/*
 * Copyright (C) 2018-2021 Canonical, Ltd.
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

#include "backend_utils.h"
#include "dbus_wrappers.h"
#include "process_factory.h"
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/process/process.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/utils.h>

#include <multipass/format.h>

#include <shared/shared_backend_utils.h>

#include <QCoreApplication>
#include <QDBusMetaType>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QSysInfo>
#include <QtDBus/QtDBus>

#include <cassert>
#include <chrono>
#include <exception>
#include <random>

#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
std::default_random_engine gen;
std::uniform_int_distribution<int> dist{0, 255};

bool subnet_used_locally(const std::string& subnet)
{
    // CLI equivalent: ip -4 route show | grep -q ${SUBNET}
    const auto output = QString::fromStdString(mp::utils::run_cmd_for_output("ip", {"-4", "route", "show"}));
    return output.contains(QString::fromStdString(subnet));
}

bool can_reach_gateway(const std::string& ip)
{
    return mp::utils::run_cmd_for_status("ping", {"-n", "-q", ip.c_str(), "-c", "-1", "-W", "1"});
}

auto virtual_switch_subnet(const QString& bridge_name)
{
    // CLI equivalent: ip -4 route show | grep ${BRIDGE_NAME} | cut -d ' ' -f1 | cut -d '.' -f1-3
    QString subnet;

    const auto output =
        QString::fromStdString(mp::utils::run_cmd_for_output("ip", {"-4", "route", "show"})).split('\n');
    for (const auto& line : output)
    {
        if (line.contains(bridge_name))
        {
            subnet = line.section('.', 0, 2);
            break;
        }
    }

    if (subnet.isNull())
    {
        mpl::log(mpl::Level::info, "daemon",
                 fmt::format("Unable to determine subnet for the {} subnet", qUtf8Printable(bridge_name)));
    }
    return subnet.toStdString();
}

} // namespace

std::string mp::backend::generate_random_subnet()
{
    gen.seed(std::chrono::system_clock::now().time_since_epoch().count());
    for (auto i = 0; i < 100; ++i)
    {
        auto subnet = fmt::format("10.{}.{}", dist(gen), dist(gen));
        if (subnet_used_locally(subnet))
            continue;

        if (can_reach_gateway(fmt::format("{}.1", subnet)))
            continue;

        if (can_reach_gateway(fmt::format("{}.254", subnet)))
            continue;

        return subnet;
    }

    throw std::runtime_error("Could not determine a subnet for networking.");
}

std::string mp::backend::get_subnet(const mp::Path& network_dir, const QString& bridge_name)
{
    auto subnet = virtual_switch_subnet(bridge_name);
    if (!subnet.empty())
        return subnet;

    QFile subnet_file{network_dir + "/multipass_subnet"};
    subnet_file.open(QIODevice::ReadWrite | QIODevice::Text);
    if (subnet_file.size() > 0)
        return subnet_file.readAll().trimmed().toStdString();

    auto new_subnet = mp::backend::generate_random_subnet();
    subnet_file.write(new_subnet.data(), new_subnet.length());
    return new_subnet;
}

void mp::backend::resize_instance_image(const MemorySize& disk_space, const mp::Path& image_path)
{
    auto disk_size = QString::number(disk_space.in_bytes()); // format documented in `man qemu-img` (look for "size")
    QStringList qemuimg_parameters{{"resize", image_path, disk_size}};
    auto qemuimg_process =
        mp::platform::make_process(std::make_unique<mp::QemuImgProcessSpec>(qemuimg_parameters, "", image_path));

    auto process_state = qemuimg_process->execute(mp::backend::image_resize_timeout);
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Cannot resize instance image: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(),
                                             qemuimg_process->read_all_standard_error()));
    }
}

mp::Path mp::backend::convert_to_qcow_if_necessary(const mp::Path& image_path)
{
    // Check if raw image file, and if so, convert to qcow2 format.
    // TODO: we could support converting from other the image formats that qemu-img can deal with
    const auto qcow2_path{image_path + ".qcow2"};

    auto qemuimg_info_spec =
        std::make_unique<mp::QemuImgProcessSpec>(QStringList{"info", "--output=json", image_path}, image_path);
    auto qemuimg_info_process = MP_PROCFACTORY.create_process(std::move(qemuimg_info_spec));

    auto process_state = qemuimg_info_process->execute();
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Cannot read image format: qemu-img failed ({}) with output:\n{}",
                                             process_state.failure_message(),
                                             qemuimg_info_process->read_all_standard_error()));
    }

    auto image_info = qemuimg_info_process->read_all_standard_output();
    auto image_record = QJsonDocument::fromJson(QString(image_info).toUtf8(), nullptr).object();

    if (image_record["format"].toString() == "raw")
    {
        auto qemuimg_convert_spec = std::make_unique<mp::QemuImgProcessSpec>(
            QStringList{"convert", "-p", "-O", "qcow2", image_path, qcow2_path}, image_path, qcow2_path);
        auto qemuimg_convert_process = MP_PROCFACTORY.create_process(std::move(qemuimg_convert_spec));
        process_state = qemuimg_convert_process->execute(mp::backend::image_resize_timeout);

        if (!process_state.completed_successfully())
        {
            throw std::runtime_error(
                fmt::format("Failed to convert image format: qemu-img failed ({}) with output:\n{}",
                            process_state.failure_message(), qemuimg_convert_process->read_all_standard_error()));
        }
        return qcow2_path;
    }
    else
    {
        return image_path;
    }
}

QString mp::backend::cpu_arch()
{
    const QHash<QString, QString> cpu_to_arch{{"x86_64", "x86_64"}, {"arm", "arm"},   {"arm64", "aarch64"},
                                              {"i386", "i386"},     {"power", "ppc"}, {"power64", "ppc64le"},
                                              {"s390x", "s390x"}};

    return cpu_to_arch.value(QSysInfo::currentCpuArchitecture());
}

void mp::backend::check_for_kvm_support()
{
    QProcess check_kvm;
    check_kvm.setProcessChannelMode(QProcess::MergedChannels);
    check_kvm.start(QDir(QCoreApplication::applicationDirPath()).filePath("check_kvm_support"));
    check_kvm.waitForFinished();

    if (check_kvm.error() == QProcess::FailedToStart)
    {
        throw std::runtime_error("The check_kvm_support script failed to start. Ensure it is in multipassd's PATH.");
    }

    if (check_kvm.exitCode() == 1)
    {
        throw std::runtime_error(check_kvm.readAll().trimmed().toStdString());
    }
}

void mp::backend::check_if_kvm_is_in_use()
{
    auto kvm_fd = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    auto ret = ioctl(kvm_fd, KVM_CREATE_VM, (unsigned long)0);

    close(kvm_fd);

    if (ret == -1 && errno == EBUSY)
        throw std::runtime_error("Another virtual machine manager is currently running. Please shut it down before "
                                 "starting a Multipass instance.");

    close(ret);
}

typedef QMap<QString, QVariantMap> VariantMapMap;
Q_DECLARE_METATYPE(VariantMapMap)

void mp::backend::create_bridge_with(const std::string& interface)
{
    static const auto nm_bus_name = QStringLiteral("org.freedesktop.NetworkManager");
    static const auto nm_obj_name = QStringLiteral("/org/freedesktop/NetworkManager/Settings");
    static const auto nm_ifc_name = QStringLiteral("org.freedesktop.NetworkManager.Settings");

    qDBusRegisterMetaType<VariantMapMap>(); // TODO@ricab move this or run once only
    const auto& system_bus = dbus::DBusProvider::instance().get_system_bus();
    if (!system_bus.is_connected())
        throw std::runtime_error{
            "Could not create bridge: failed to connect to D-Bus system bus:"}; // TODO@ricab create exception digesting
                                                                                // QDBusError

    auto nm_settings = system_bus.get_interface(nm_bus_name, nm_obj_name, nm_ifc_name);

    assert(nm_settings);
    if (!nm_settings->is_valid())
        throw std::runtime_error{"TODO"}; // TODO@ricab

    // TODO@ricab verify if suitable bridge exists
    // TODO@ricab derive new bridge name

    // Expected DBus argument type: a{sa{sv}}
    VariantMapMap arg{{"connection", {{"type", QVariant{"bridge"}}, {"id", QVariant{"qtbr0"}}}},
                      {"bridge", {{"interface-name", QVariant{"qtbr0"}}}}};
    QDBusReply<QDBusObjectPath> obj = nm_settings->call(QDBus::Block, "AddConnection", QVariant::fromValue(arg));
    if (!obj.isValid())
    {
        throw std::runtime_error{fmt::format("Could not create bridge: {}", obj.error().message())}; // TODO@ricab
        // TODO@ricab: the bridge could already be there (e.g. disconnect after creation), so revert
    }

    // TODO@ricab create second connection, then activate the bridge
}
