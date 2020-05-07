/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
const QStringList select_object{"|", "Select-Object", "-ExpandProperty"};

void ensure_hyperv_service_is_running(mp::PowerShell& power_shell)
{
    QString ps_output;
    const QStringList get_vmms_service{"Get-Service", "-Name", "vmms"};

    if (!power_shell.run(QStringList() << get_vmms_service << select_object << "Status", ps_output))
    {
        throw std::runtime_error("The Hyper-V service does not exist. Ensure Hyper-V is installed correctly.");
    }

    if (ps_output == "Stopped")
    {
        power_shell.run(QStringList() << get_vmms_service << select_object << "StartType", ps_output);

        if (ps_output == "Disabled")
        {
            throw std::runtime_error("The Hyper-V service is set to disabled. Please re-enable \"vmms\".");
        }

        if (!power_shell.run(QStringList() << "Start-Service"
                                           << "-Name"
                                           << "vmms",
                             ps_output))
        {
            throw std::runtime_error("Could not start the Hyper-V service");
        }
    }
}

void check_host_hyperv_support(mp::PowerShell& power_shell)
{
    QString ps_output;

    if (power_shell.run(QStringList() << "Get-CimInstance Win32_Processor" << select_object
                                      << "VirtualizationFirmwareEnabled",
                        ps_output))
    {
        if (ps_output == "False")
        {
            throw std::runtime_error("Virtualization support appears to be disabled in the BIOS.\n"
                                     "Enter your BIOS setup and enable Virtualization Technology (VT).");
        }
    }

    if (power_shell.run(QStringList() << "Get-CimInstance Win32_Processor" << select_object
                                      << "SecondLevelAddressTranslationExtensions",
                        ps_output))
    {
        if (ps_output == "False")
        {
            throw std::runtime_error("The CPU does not have proper virtualization extensions to support Hyper-V");
        }
    }
}

void check_hyperv_feature_enabled(mp::PowerShell& power_shell)
{
    QString ps_output;
    const QStringList optional_feature{"Get-WindowsOptionalFeature", "-Online", "-FeatureName"};

    // Check if Hyper-V is fully enabled
    if (power_shell.run(QStringList() << optional_feature << "Microsoft-Hyper-V" << select_object << "State",
                        ps_output))
    {
        if (ps_output.isEmpty())
        {
            throw std::runtime_error(
                "Hyper-V is not available on this edition of Windows 10. Please upgrade to one of Pro, Enterprise"
                "or Education editions.");
        }
        else if (ps_output == "Enabled")
        {
            power_shell.run(QStringList()
                                << optional_feature << "Microsoft-Hyper-V-Hypervisor" << select_object << "State",
                            ps_output);
            if (ps_output == "Enabled")
                return;

            throw std::runtime_error("The Hyper-V Hypervisor is disabled. Please enable by using the following\n"
                                     "command in an Administrator Powershell and reboot:\n"
                                     "Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-Hypervisor");
        }

        throw std::runtime_error("The Hyper-V Windows feature is disabled. Please enable by using the following\n"
                                 "command in an Administrator Powershell and reboot:\n"
                                 "Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V -All");
    }
    else
    {
        throw std::runtime_error("Cannot determine if Hyper-V is available on this system.");
    }
}

void check_hyperv_support()
{
    mp::PowerShell power_shell("Hyper-V Health Check");
    QString ps_output;
    QStringList get_reg_version_info{"Get-ItemProperty", "-Path",
                                     "'HKLM:\\Software\\Microsoft\\Windows NT\\CurrentVersion'"};

    // Check for Windows 10
    power_shell.run(QStringList() << get_reg_version_info << select_object << "CurrentMajorVersionNumber", ps_output);
    if (ps_output != "10")
        throw std::runtime_error("Multipass support for Hyper-V requires Windows 10");

    // Check if it's a version less than 1803
    power_shell.run(QStringList() << get_reg_version_info << select_object << "ReleaseId", ps_output);
    if (ps_output.toInt() < 1803)
        throw std::runtime_error("Multipass requires at least Windows 10 version 1803. Please update your system.");

    // Check if HypervisorPresent is true- implies either Hyper-V is running or running under a
    //   different virtualized environment like VirtualBox or QEMU.
    // If it's running under a different virtualized environment, we can't check if nesting is
    //   available, so the user is on their own and we'll bubble up any failures at `launch`.
    power_shell.run(QStringList() << "Get-CimInstance Win32_ComputerSystem" << select_object << "HypervisorPresent",
                    ps_output);

    QString hypervisor_present{ps_output};
    // Implies Hyper-V is not running (or any hypervisor for that matter).
    // Determine why it is not running.
    if (hypervisor_present == "False")
    {
        // First check if the CPU has the proper virtualization support.
        // This is only accurate when "HypervisorPresent" returns false.
        // This throws if the support is not found.
        check_host_hyperv_support(power_shell);
    }

    // Check if the Hyper-V feature is enabled.
    // Throws if the Hyper-V feature is not enabled.
    check_hyperv_feature_enabled(power_shell);

    // Check to make sure the service is running.
    // Throws when it's not running.
    ensure_hyperv_service_is_running(power_shell);

    // Lastly, if we make it this far, check hypervisor_present again.
    // If it's false, then we know Hyper-V is enabled, but the host has
    // not rebooted yet.
    if (hypervisor_present == "False")
    {
        throw std::runtime_error("The computer needs to be rebooted in order for Hyper-V to be fully available");
    }
}
} // namespace

mp::VirtualMachine::UPtr mp::HyperVVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                                 VMStatusMonitor& monitor)
{
    return std::make_unique<mp::HyperVVirtualMachine>(desc, monitor);
}

void mp::HyperVVirtualMachineFactory::remove_resources_for(const std::string& name)
{
    PowerShell::exec({"Remove-VM", "-Name", QString::fromStdString(name), "-Force"}, name);
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
    auto disk_size = QString::number(desc.disk_space.in_bytes()); // format documented in `Help(Resize-VHD)`
    PowerShell::exec({"Resize-VHD", "-Path", instance_image.image_path, "-SizeBytes", disk_size}, desc.vm_name);
}

void mp::HyperVVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config,
                                                YAML::Node& user_config)
{
}

void mp::HyperVVirtualMachineFactory::hypervisor_health_check()
{
    check_hyperv_support();
}
