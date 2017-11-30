/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "hyperv_virtual_machine_factory.h"
#include "hyperv_virtual_machine.h"
#include "powershell.h"

#include <multipass/virtual_machine_description.h>

#include <yaml-cpp/yaml.h>

#include <QFileInfo>
#include <QProcess>

namespace mp = multipass;

namespace
{
void create_virtual_switch()
{
    if (!mp::powershell_run({"Get-VMSwitch", "-Name", "multipass"}))
    {
        mp::powershell_run({"New-VMSwitch", "-SwitchType", "Internal", "-Name", "multipass"});
        mp::powershell_run({"New-NetIPAddress", "-IPAddress", "10.122.122.1", "-PrefixLength", "24", "-InterfaceIndex",
                            "(Get-NetAdapter -Name 'vEthernet (multipass)').ifIndex"});
        mp::powershell_run(
            {"New-NetNat", "-Name", "multipassNAT", "-InternalIPInterfaceAddressPrefix", "10.122.122.0/24"});
    }
}
}

mp::HyperVVirtualMachineFactory::HyperVVirtualMachineFactory(const mp::Path& data_dir)
    : ip_pool{data_dir, mp::IPAddress{"10.122.122.2"}, mp::IPAddress{"10.122.122.254"}}
{
}

mp::VirtualMachine::UPtr mp::HyperVVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                                 VMStatusMonitor& monitor)
{
    create_virtual_switch();
    return std::make_unique<mp::HyperVVirtualMachine>(ip_pool.obtain_ip_for(desc.vm_name), desc);
}

void mp::HyperVVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    ip_pool.remove_ip_for(name);
    powershell_run({"Remove-VM", "-Name", QString::fromStdString(name), "-Force"});
}

mp::FetchType mp::HyperVVirtualMachineFactory::fetch_type()
{
    return mp::FetchType::ImageOnly;
}

mp::VMImage mp::HyperVVirtualMachineFactory::prepare_source_image(const mp::VMImage& source_image)
{
    QFileInfo source_file{source_image.image_path};
    auto vhdx_file = QString("%1/%2.vhdx").arg(source_file.path()).arg(source_file.completeBaseName());

    QStringList convert_args(
        {QStringLiteral("convert"), "-o", "subformat=dynamic", "-O", "vhdx", source_image.image_path, vhdx_file});

    QProcess convert;
    convert.setProgram("qemu-img.exe");
    convert.setArguments(convert_args);
    convert.start();
    convert.waitForFinished();

    if (convert.exitCode() != QProcess::NormalExit)
    {
        throw std::runtime_error(
            qPrintable("Conversion of image to vhdx failed with error: " + convert.readAllStandardError()));
    }
    if (!QFile::exists(vhdx_file))
    {
        throw std::runtime_error("vhdx image file is missing");
    }

    auto prepared_image = source_image;
    prepared_image.image_path = vhdx_file;
    return prepared_image;
}

void mp::HyperVVirtualMachineFactory::prepare_instance_image(const mp::VMImage& instance_image,
                                                             const VirtualMachineDescription& desc)
{
    if (!desc.disk_space.empty())
    {
        auto disk_space = QString::fromStdString(desc.disk_space);
        if (disk_space.endsWith('K') || disk_space.endsWith('M') || disk_space.endsWith('G'))
            disk_space.append("B");
        powershell_run({"Resize-VHD", "-Path", instance_image.image_path, "-SizeBytes", disk_space});
    }
}

void mp::HyperVVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config,
                                                YAML::Node& user_config)
{
    meta_config["dsmode"] = "local";

    auto ip = ip_pool.obtain_ip_for(name);
    std::stringstream netconfig;

    netconfig << "|\n"
              << "    auto eth0\n"
              << "    iface eth0 inet static\n"
              << "    address " << ip.as_string() << "\n"
              << "    network 10.122.122.0\n"
              << "    netmask 255.255.255.0\n"
              << "    broadcast 10.122.122.255\n"
              << "    gateway 10.122.122.1\n"
              << "    dns-nameservers 8.8.8.8\n"; // TODO: Use a DNS proxy instead

    meta_config["network-interfaces"] = netconfig.str();
}
