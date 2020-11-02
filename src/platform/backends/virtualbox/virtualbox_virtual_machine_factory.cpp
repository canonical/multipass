/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include "virtualbox_virtual_machine_factory.h"
#include "virtualbox_virtual_machine.h"

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface_info.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>

#include <shared/shared_backend_utils.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

mp::VirtualMachine::UPtr
mp::VirtualBoxVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                            VMStatusMonitor& monitor)
{
    return std::make_unique<mp::VirtualBoxVirtualMachine>(desc, monitor);
}

void mp::VirtualBoxVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    QRegularExpression cloudinit_re("\"SATA_0-1-0\"=\"(?!emptydrive)(.+)\"");

    QProcess vminfo;
    vminfo.start("VBoxManage", {"showvminfo", QString::fromStdString(name), "--machinereadable"});
    vminfo.waitForFinished();
    auto vminfo_output = QString::fromUtf8(vminfo.readAllStandardOutput());

    auto cloudinit_match = cloudinit_re.match(vminfo_output);

    mpu::process_log_on_error("VBoxManage", {"controlvm", QString::fromStdString(name), "poweroff"},
                              "Could not power off VM: {}", QString::fromStdString(name), mpl::Level::warning);
    mpu::process_log_on_error("VBoxManage", {"unregistervm", QString::fromStdString(name), "--delete"},
                              "Could not unregister VM: {}", QString::fromStdString(name), mpl::Level::error);

    if (cloudinit_match.hasMatch())
    {
        mpu::process_log_on_error("VBoxManage", {"closemedium", "dvd", cloudinit_match.captured(1)},
                                  "Could not unregister cloud-init medium: {}", QString::fromStdString(name),
                                  mpl::Level::warning);
    }
    else
    {
        mpl::log(mpl::Level::warning, name, "Could not find the cloud-init ISO path for removal.");
    }
}

mp::VMImage mp::VirtualBoxVirtualMachineFactory::prepare_source_image(const mp::VMImage& source_image)
{
    QFileInfo source_file{source_image.image_path};
    auto vdi_file = QString("%1/%2.vdi").arg(source_file.path()).arg(source_file.completeBaseName());

    QStringList convert_args({"convert", "-O", "vdi", source_image.image_path, vdi_file});

    auto qemuimg_convert_spec =
        std::make_unique<mp::QemuImgProcessSpec>(convert_args, source_image.image_path, vdi_file);
    auto qemuimg_convert_process = mp::platform::make_process(std::move(qemuimg_convert_spec));

    auto process_state = qemuimg_convert_process->execute(mp::backend::image_resize_timeout);
    if (!process_state.completed_successfully())
    {
        throw std::runtime_error(fmt::format("Conversion of image to VDI failed ({}) with the following output:\n{}",
                                             process_state.failure_message(),
                                             qemuimg_convert_process->read_all_standard_error()));
    }

    if (!QFile::exists(vdi_file))
    {
        throw std::runtime_error("vdi image file is missing");
    }

    auto prepared_image = source_image;
    prepared_image.image_path = vdi_file;
    return prepared_image;
}

void mp::VirtualBoxVirtualMachineFactory::prepare_instance_image(const mp::VMImage& instance_image,
                                                                 const VirtualMachineDescription& desc)
{
    // Need to generate a new medium UUID
    mpu::process_throw_on_error("VBoxManage", {"internalcommands", "sethduuid", instance_image.image_path},
                                "Could not generate a new UUID: {}");

    mpu::process_log_on_error(
        "VBoxManage",
        {"modifyhd", instance_image.image_path, "--resize", QString::number(desc.disk_space.in_megabytes())},
        "Could not resize image: {}", QString::fromStdString(desc.vm_name), mpl::Level::warning);
}

void mp::VirtualBoxVirtualMachineFactory::hypervisor_health_check()
{
}

mp::NetworkInterfaceInfo get_backend_only_interface_information(const QString& vbox_iface_info)
{
    // These patterns are intended to gather from VBoxManage output the information we need.
    const auto name_pattern = QStringLiteral("^Name: +(?<name>[A-Za-z0-9-_#\\+ ]+)$.*");
    const auto type_pattern = QStringLiteral("^MediumType: +(?<type>\\w+)$.*");
    const auto wireless_pattern = QStringLiteral("^Wireless: +(?<wireless>\\w+)$");

    const auto regexp_options = QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption;

    const auto name_regexp = QRegularExpression{name_pattern, regexp_options};
    const auto type_regexp = QRegularExpression{type_pattern, regexp_options};
    const auto wireless_regexp = QRegularExpression{wireless_pattern, regexp_options};

    const auto name_match = name_regexp.match(vbox_iface_info);

    if (name_match.hasMatch())
    {
        std::string ifname = name_match.captured("name").toStdString();
        std::string iftype = type_regexp.match(vbox_iface_info).captured("type").toStdString();
        bool wireless = wireless_regexp.match(vbox_iface_info).captured("wireless") == "Yes";

        if (iftype == "Ethernet")
        {
            if (wireless)
                return mp::NetworkInterfaceInfo{ifname, "wifi", "Wi-Fi device"};
            else
                return mp::NetworkInterfaceInfo{ifname, "ethernet", "Wired or virtual device"};
        }
        else
            return mp::NetworkInterfaceInfo{ifname, wireless ? "wireless" : "wired", iftype};
    }

    return mp::NetworkInterfaceInfo{"", "", ""};
}

mp::NetworkInterfaceInfo list_vbox_network(const QString& vbox_iface_info,
                                           std::map<std::string, mp::NetworkInterfaceInfo> platform_info)
{
    // The Mac version of VBoxManage is the only one which gives us the <description> field for some devices.
    const auto name_pattern =
        QStringLiteral("^Name: +(?<name>[A-Za-z0-9-_#\\+ ]+)(: (?<description>[A-Za-z0-9-_()\\? :]+))?\r?$.*");
    const auto type_pattern = QStringLiteral("^MediumType: +(?<type>\\w+)\r?$.*");
    const auto wireless_pattern = QStringLiteral("^Wireless: +(?<wireless>\\w+)\r?$");

    const auto regexp_options = QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption;

    const auto name_regexp = QRegularExpression{name_pattern, regexp_options};
    const auto type_regexp = QRegularExpression{type_pattern, regexp_options};
    const auto wireless_regexp = QRegularExpression{wireless_pattern, regexp_options};

    const auto name_match = name_regexp.match(vbox_iface_info);

    if (name_match.hasMatch())
    {
        std::string ifname = name_match.captured("name").toStdString();

        auto platform_if_info = platform_info.find(ifname);

        // In Windows, VirtualBox lists interfaces using their description as name. Thus, if the map returned
        // by the OS does not contain our given name, we should iterate over the map values to find the
        // description we've been given.
        if (platform_if_info == platform_info.end()) // This will be be true until VirtualBox fixes the issue.
        {
            for (auto map_it = platform_info.begin(); map_it != platform_info.end(); ++map_it)
            {
                if (map_it->second.description == ifname)
                {
                    platform_if_info = map_it;
                    break;
                }
            }
        }

        if (platform_if_info != platform_info.end())
        {
            std::string iftype = type_regexp.match(vbox_iface_info).captured("type").toStdString();
            std::string ifdescription = name_match.captured("description").toStdString();
            bool wireless = wireless_regexp.match(vbox_iface_info).captured("wireless") == "Yes";

            mp::NetworkInterfaceInfo if_info = platform_if_info->second;

            if (ifdescription.empty())
            {
                // Use the OS information about the interface. But avoid adding unknown virtual interfaces,
                // which cannot be bridged.
                if (!(if_info.type == "virtual" && if_info.description == "unknown"))
                    return mp::NetworkInterfaceInfo{
                        if_info.id, wireless ? "wifi" : (if_info.type.empty() ? "unknown" : if_info.type),
                        if_info.description};
            }
            else
            {
                // Get the information from the VBoxManage output.
                iftype = wireless ? "wifi" : (ifdescription.compare(0, 11, "Thunderbolt") ? "thunderbolt" : iftype);

                return mp::NetworkInterfaceInfo{if_info.id, iftype, ifdescription};
            }
        }
    }

    return mp::NetworkInterfaceInfo{"", "", ""};
}

auto mp::VirtualBoxVirtualMachineFactory::list_networks() const -> std::vector<NetworkInterfaceInfo>
{
    std::vector<NetworkInterfaceInfo> networks;

    // Get the list of all the interfaces which can be bridged by VirtualBox.
    QString ifs_info = QString::fromStdString(mpu::run_cmd_for_output("VBoxManage", {"list", "-l", "bridgedifs"}));

    // List to store the output of the query command; each element corresponds to one interface.
    QStringList if_list(ifs_info.split(QRegularExpression("\r?\n\r?\n"), QString::SkipEmptyParts));

    mpl::log(mpl::Level::info, "list-networks", fmt::format("VirtualBox found {} interfaces", if_list.size()));

    std::map<std::string, mp::NetworkInterfaceInfo> platform_ifs_info = mp::platform::get_network_interfaces_info();

    for (const auto& iface : if_list)
    {
        auto interface_info = list_vbox_network(iface, platform_ifs_info);

        if (!interface_info.type.empty())
            networks.push_back(interface_info);
    }

    return networks;
}

// The day VirtualBox corrects the bug in Windows which avoids us to use the correct interface name instead of the
// description, we'll have to modify this function and check the VirtualBox version.
std::string mp::VirtualBoxVirtualMachineFactory::low_level_id(const std::string& ux_id) const
{
#ifdef MULTIPASS_PLATFORM_WINDOWS
    // Get information about all the interfaces.
    auto if_info = mp::platform::get_network_interface_info(ux_id);

    if (if_info.id == "")
        throw std::runtime_error(fmt::format("Network interface \"{}\" not found", ux_id));
    else
        return if_info.description;
#else
    return ux_id;
#endif
}
