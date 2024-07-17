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

#include "virtualbox_virtual_machine_factory.h"
#include "virtualbox_virtual_machine.h"

#include <multipass/cloud_init_iso.h>
#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface_info.h>
#include <multipass/platform.h>
#include <multipass/process/qemuimg_process_spec.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{

struct VirtualBoxNetworkException : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

mp::NetworkInterfaceInfo list_vbox_network(const QString& vbox_iface_info,
                                           const std::map<std::string, mp::NetworkInterfaceInfo>& platform_info)
{
    // The Mac version of VBoxManage is the only one which gives us the <description> field for some devices.
    const auto name_pattern = QStringLiteral("^Name: +(?<name>.+?)(: (?<description>.+))?\r?$");
    const auto type_pattern = QStringLiteral("^MediumType: +(?<type>\\w+)\r?$");
    const auto wireless_pattern = QStringLiteral("^Wireless: +(?<wireless>\\w+)\r?$");

    const auto regexp_options = QRegularExpression::MultilineOption;

    const auto name_regexp = QRegularExpression{name_pattern, regexp_options};
    const auto type_regexp = QRegularExpression{type_pattern, regexp_options};
    const auto wireless_regexp = QRegularExpression{wireless_pattern, regexp_options};

    const auto name_match = name_regexp.match(vbox_iface_info);

    // If the name does not match, we know there is something strange in the input, so we throw. If it matches, we
    // see if the interface is useful for us and the platform recognizes it; otherwise we throw as well.
    if (name_match.hasMatch())
    {
        std::string ifname = name_match.captured("name").toStdString();

        auto platform_if_info = platform_info.find(ifname);

        // In Windows, VirtualBox lists interfaces using their description as name.
        if (platform_if_info == platform_info.end()) // This will be true until VirtualBox fixes the issue.
        {
            auto comp_fun = [&ifname](const auto& keyval) { return keyval.second.description == ifname; };
            platform_if_info = std::find_if(platform_info.begin(), platform_info.end(), comp_fun);
        }

        if (platform_if_info != platform_info.end())
        {
            std::string iftype = type_regexp.match(vbox_iface_info).captured("type").toStdString();
            std::string ifdescription = name_match.captured("description").toStdString();
            bool wireless = wireless_regexp.match(vbox_iface_info).captured("wireless") == "Yes";

            mp::NetworkInterfaceInfo if_info = platform_if_info->second;

            if (ifdescription.empty())
            {
                // Use the OS information about the interface
                return mp::NetworkInterfaceInfo{if_info.id,
                                                wireless ? "wifi" : (if_info.type.empty() ? "unknown" : if_info.type),
                                                if_info.description};
            }
            else
            {
                // Get the information from the VBoxManage output.
                iftype = wireless ? "wifi" : (ifdescription.compare(0, 11, "Thunderbolt") ? iftype : "thunderbolt");

                return mp::NetworkInterfaceInfo{if_info.id, iftype, ifdescription};
            }
        }

        throw VirtualBoxNetworkException(fmt::format("Network interface \"{}\" not recognized by platform", ifname));
    }

    throw std::runtime_error(fmt::format("Unexpected data from VBoxManage: \"{}\"", vbox_iface_info));
}

namespace fs = std::filesystem;
void copy_instance_dir_with_cloud_init_file_only(const fs::path& source_instance_dir_path,
                                                 const fs::path& dest_instance_dir_path)
{
    if (fs::exists(source_instance_dir_path) && fs::is_directory(source_instance_dir_path))
    {
        for (const auto& entry : fs::directory_iterator(source_instance_dir_path))
        {
            fs::create_directory(dest_instance_dir_path);

            // we only need cloud-init-config.iso file here, becaue the configuration files and image file will be
            // copied by the VBoxManage clonevm command.
            if (entry.path().extension().string() == ".iso")
            {
                const fs::path dest_file_path = dest_instance_dir_path / entry.path().filename();
                fs::copy(entry.path(), dest_file_path, fs::copy_options::update_existing);
            }
        }
    }
}

} // namespace

mp::VirtualBoxVirtualMachineFactory::VirtualBoxVirtualMachineFactory(const mp::Path& data_dir)
    : BaseVirtualMachineFactory(MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir))
{
}

auto mp::VirtualBoxVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                 const SSHKeyProvider& key_provider,
                                                                 VMStatusMonitor& monitor) -> mp::VirtualMachine::UPtr
{
    return std::make_unique<mp::VirtualBoxVirtualMachine>(desc,
                                                          monitor,
                                                          key_provider,
                                                          get_instance_directory(desc.vm_name));
}

void mp::VirtualBoxVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
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

    auto process_state = qemuimg_convert_process->execute(mp::image_resize_timeout);
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

mp::VirtualMachine::UPtr mp::VirtualBoxVirtualMachineFactory::create_vm_and_clone_instance_dir_data(
    const VMSpecs& /*src_vm_spec*/,
    const VMSpecs& dest_vm_spec,
    const std::string& source_name,
    const std::string& destination_name,
    const VMImage& dest_vm_image,
    const SSHKeyProvider& key_provider,
    VMStatusMonitor& monitor)
{
    const std::filesystem::path source_instance_data_directory{get_instance_directory(source_name).toStdString()};
    const std::filesystem::path dest_instance_data_directory{get_instance_directory(destination_name).toStdString()};

    copy_instance_dir_with_cloud_init_file_only(source_instance_data_directory, dest_instance_data_directory);

    const fs::path cloud_init_config_iso_file_path = dest_instance_data_directory / "cloud-init-config.iso";

    MP_CLOUD_INIT_FILE_OPS.update_cloned_cloud_init_unique_identifiers(dest_vm_spec.default_mac_address,
                                                                       dest_vm_spec.extra_interfaces,
                                                                       destination_name,
                                                                       cloud_init_config_iso_file_path);

    // start to construct VirtualMachineDescription
    mp::VirtualMachineDescription dest_vm_desc{dest_vm_spec.num_cores,
                                               dest_vm_spec.mem_size,
                                               dest_vm_spec.disk_space,
                                               destination_name,
                                               dest_vm_spec.default_mac_address,
                                               dest_vm_spec.extra_interfaces,
                                               dest_vm_spec.ssh_username,
                                               dest_vm_image,
                                               cloud_init_config_iso_file_path.string().c_str(),
                                               {},
                                               {},
                                               {},
                                               {}};

    mp::VirtualMachine::UPtr cloned_instance =
        std::make_unique<mp::VirtualBoxVirtualMachine>(source_name,
                                                       dest_vm_desc,
                                                       monitor,
                                                       key_provider,
                                                       get_instance_directory(dest_vm_desc.vm_name));
    cloned_instance->remove_all_snapshots_from_the_image();

    return cloned_instance;
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

auto mp::VirtualBoxVirtualMachineFactory::networks() const -> std::vector<NetworkInterfaceInfo>
{
    std::string log_category("virtualbox factory");

    std::vector<NetworkInterfaceInfo> networks;

    // Get the list of all the interfaces which can be bridged by VirtualBox.
    QString ifs_info = QString::fromStdString(MP_UTILS.run_cmd_for_output("VBoxManage", {"list", "-l", "bridgedifs"}));

    // List to store the output of the query command; each element corresponds to one interface.
    QStringList if_list(ifs_info.split(QRegularExpression("\r?\n\r?\n"), Qt::SkipEmptyParts));

    mpl::log(mpl::Level::info, log_category, fmt::format("VirtualBox found {} interface(s)", if_list.size()));

    std::map<std::string, mp::NetworkInterfaceInfo> platform_ifs_info = MP_PLATFORM.get_network_interfaces_info();

    for (const auto& iface : if_list)
    {
        try
        {
            auto interface_info = list_vbox_network(iface, platform_ifs_info);

            networks.push_back(interface_info);
        }
        catch (VirtualBoxNetworkException& e)
        {
            mpl::log(mpl::Level::warning, log_category, e.what());
        }
    }

    return networks;
}

void multipass::VirtualBoxVirtualMachineFactory::prepare_networking(std::vector<NetworkInterface>& vector)
{
    // Nothing to do here, VirtualBox takes host interfaces directly
}
