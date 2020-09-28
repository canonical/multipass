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

auto mp::VirtualBoxVirtualMachineFactory::list_networks() const -> std::vector<NetworkInterfaceInfo>
{
    std::vector<NetworkInterfaceInfo> networks;

    // Get the information about the host network interfaces.
    auto platform_ifs_info = mp::platform::get_network_interfaces_info();

    // Get the list of all the interfaces which can be bridged by VirtualBox.
    QString ifs_info = QString::fromStdString(mpu::run_cmd_for_output("VBoxManage", {"list", "-l", "bridgedifs"}));

    // List to store the output of the query command; each element corresponds to one interface.
    QStringList if_list(ifs_info.split(QRegularExpression("\r?\n\r?\n"), QString::SkipEmptyParts));

    mpl::log(mpl::Level::info, "list-networks", fmt::format("Found {} network interfaces", if_list.size()));

    // This pattern is intended to gather from VBoxManage output the information we need. However, only the Mac
    // version gives us enough information (and for some interfaces only). This extra information provided by the
    // Mac version is captured in <description>.
    const auto pattern =
        QStringLiteral("^Name: +(?<name>[A-Za-z0-9-_#\\+ ]+)(: (?<description>[A-Za-z0-9-_()\\? :]+))?\r?$.*"
                       "^MediumType: +(?<type>\\w+)\r?$.*^Wireless: +(?<wireless>\\w+)\r?$");
    const auto regexp = QRegularExpression{pattern, QRegularExpression::MultilineOption |
                                                        QRegularExpression::DotMatchesEverythingOption};

    std::string ifname, iftype, ifdescription;

    // For each interface in the list, see if VBoxManage gave us enough information. If not, ask the OS.
    for (auto iface : if_list)
    {
        const auto match = regexp.match(iface);

        if (match.hasMatch())
        {
            ifname = match.captured("name").toStdString();
            auto platform_if_info = platform_ifs_info.find(ifname);

#ifdef MULTIPASS_PLATFORM_WINDOWS
            // In Windows, VirtualBox lists interfaces using their description as name. Thus, if the map returned
            // by the OS does not contain our given name, we should iterate over the map values to find the
            // description we've been given.
            if (platform_if_info == platform_ifs_info.end()) // This will be be true until VirtualBox fixes the issue.
            {
                for (auto map_it = platform_ifs_info.begin(); map_it != platform_ifs_info.end(); ++map_it)
                {
                    if (map_it->second.description == ifname)
                    {
                        platform_if_info = map_it;
                        break;
                    }
                }
            }
#endif

            // Show this interface only if the OS knows about it.
            if (platform_if_info != platform_ifs_info.end())
            {
                mp::NetworkInterfaceInfo if_info = platform_if_info->second;

                // Only VirtualBox on MacOS can fill this field in. But only for some interfaces, not necessarily
                // for all. If the description is present, it takes precedence over the data from the OS.
                ifdescription = match.captured("description").toStdString();

                if (ifdescription.empty())
                {
                    // Use the OS information about the interface. But avoid adding unknown virtual interfaces,
                    // which cannot be bridged.
                    if (if_info.type != "virtual" && if_info.description != "unknown")
                    {
                        networks.push_back({if_info.id, if_info.type.empty() ? "unknown" : if_info.type,
                                            if_info.description, mp::nullopt});
                    }
                }
                else
                {
                    // Get the information from the VBoxManage output.
                    if (match.captured("wireless") == "Yes")
                    {
                        iftype = "wifi";
                    }
                    else
                    {
                        iftype = ifdescription.compare(0, 11, "Thunderbolt")
                                     ? match.captured("type").toLower().toStdString()
                                     : "thunderbolt";
                    }

                    networks.push_back({if_info.id, iftype.empty() ? "unknown" : iftype, ifdescription, mp::nullopt});
                }
            }
        }
    }

    return networks;
}

// The day VirtualBox corrects the bug in Windows which avoids us to use the correct interface name instead of the
// description, we'll have to modify this function and check the VirtualBox version.
std::string mp::VirtualBoxVirtualMachineFactory::interface_id(const std::string& user_id) const
{
#ifdef MULTIPASS_PLATFORM_WINDOWS
    // Get information about all the interfaces.
    auto if_info = mp::platform::get_network_interface_info(user_id);

    if (if_info.id == "")
        throw std::runtime_error(fmt::format("Network interface \"{}\" not found", user_id));
    else
        return if_info.description;
#else
    return user_id;
#endif
}
