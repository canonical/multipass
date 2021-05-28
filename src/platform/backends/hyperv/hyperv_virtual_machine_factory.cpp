/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include <multipass/format.h>
#include <multipass/network_interface_info.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine_description.h>

#include <shared/shared_backend_utils.h>
#include <shared/win/powershell.h>

#include <yaml-cpp/yaml.h>

#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>
#include <iterator>

namespace mp = multipass;

namespace
{
const QStringList& expand_property = mp::PowerShell::Snippets::expand_property;

void ensure_hyperv_service_is_running(mp::PowerShell& power_shell)
{
    QString ps_output;
    const QStringList get_vmms_service{"Get-Service", "-Name", "vmms"};

    if (!power_shell.run(QStringList() << get_vmms_service << expand_property << "Status", ps_output))
    {
        throw std::runtime_error("The Hyper-V service does not exist. Ensure Hyper-V is installed correctly.");
    }

    if (ps_output == "Stopped")
    {
        power_shell.run(QStringList() << get_vmms_service << expand_property << "StartType", ps_output);

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

    if (power_shell.run(QStringList() << "Get-CimInstance Win32_Processor" << expand_property
                                      << "VirtualizationFirmwareEnabled",
                        ps_output))
    {
        if (ps_output == "False")
        {
            throw std::runtime_error("Virtualization support appears to be disabled in the BIOS.\n"
                                     "Enter your BIOS setup and enable Virtualization Technology (VT).");
        }
    }

    if (power_shell.run(QStringList() << "Get-CimInstance Win32_Processor" << expand_property
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
    if (power_shell.run(QStringList() << optional_feature << "Microsoft-Hyper-V" << expand_property << "State",
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
                                << optional_feature << "Microsoft-Hyper-V-Hypervisor" << expand_property << "State",
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
    power_shell.run(QStringList() << get_reg_version_info << expand_property << "CurrentMajorVersionNumber", ps_output);
    if (ps_output != "10")
        throw std::runtime_error("Multipass support for Hyper-V requires Windows 10");

    // Check if it's a version less than 1803
    power_shell.run(QStringList() << get_reg_version_info << expand_property << "ReleaseId", ps_output);
    if (ps_output.toInt() < 1803)
        throw std::runtime_error("Multipass requires at least Windows 10 version 1803. Please update your system.");

    // Check if HypervisorPresent is true- implies either Hyper-V is running or running under a
    //   different virtualized environment like VirtualBox or QEMU.
    // If it's running under a different virtualized environment, we can't check if nesting is
    //   available, so the user is on their own and we'll bubble up any failures at `launch`.
    power_shell.run(QStringList() << "Get-CimInstance Win32_ComputerSystem" << expand_property << "HypervisorPresent",
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

std::vector<std::string> switch_links(const std::vector<mp::NetworkInterfaceInfo>& adapters,
                                      const QString& adapter_description)
{
    std::vector<std::string> ret;
    if (!adapter_description.isEmpty())
    {
        auto same = [&adapter_description](const auto& net) { return adapter_description == net.description.c_str(); };
        if (auto it = std::find_if(adapters.cbegin(), adapters.cend(), same); it != adapters.cend())
            ret.emplace_back(it->id);
    }

    return ret;
}

std::string switch_description(const QString& switch_type, const std::vector<std::string>& links)
{
    if (switch_type.contains("external", Qt::CaseInsensitive))
    {
        if (links.empty())
            return "Virtual Switch with external networking";

        return fmt::format("Virtual Switch with external networking via \"{}\"", fmt::join(links, ", "));
    }
    else if (!links.empty())
        throw std::runtime_error{fmt::format("Unexpected link(s) for non-external switch: {}", fmt::join(links, ", "))};
    else if (switch_type.contains("private", Qt::CaseInsensitive))
        return "Private virtual switch";
    else if (switch_type.contains("internal", Qt::CaseInsensitive))
        return "Virtual Switch with internal networking";
    else
        return fmt::format("Unknown Virtual Switch type: {}", switch_type);
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

    if (!convert.waitForFinished(mp::backend::image_resize_timeout))
    {
        throw std::runtime_error(
            qPrintable("Conversion of image to vhdx timed out..."));
    }

    if (convert.exitStatus() != QProcess::NormalExit || convert.exitCode() != 0)
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

void mp::HyperVVirtualMachineFactory::hypervisor_health_check()
{
    check_hyperv_support();
}

auto mp::HyperVVirtualMachineFactory::networks() const -> std::vector<NetworkInterfaceInfo>
{
    auto adapters = get_adapters();
    auto switches = get_switches(adapters);
    //    update_adapter_authorizations(adapters); // TODO@ricab

    if (adapters.size() > switches.size())
        swap(adapters, switches);                        // we want to move the smallest one
    switches.reserve(adapters.size() + switches.size()); // avoid growing more times than needed

    move(adapters.begin(), adapters.end(), back_inserter(switches));

    return switches;
}

auto mp::HyperVVirtualMachineFactory::get_switches(const std::vector<NetworkInterfaceInfo>& adapters)
    -> std::vector<NetworkInterfaceInfo>
{
    static const auto ps_cmd_base = QStringLiteral("Get-VMSwitch | Select-Object -Property Name,SwitchType,"
                                                   "NetAdapterInterfaceDescription");
    static const auto ps_args = ps_cmd_base.split(' ', QString::SkipEmptyParts) + mp::PowerShell::Snippets::to_bare_csv;

    QString ps_output;
    if (mp::PowerShell::exec(ps_args, "Hyper-V Switch Listing", ps_output))
    {
        std::vector<mp::NetworkInterfaceInfo> ret{};
        for (const auto& line : ps_output.split(QRegularExpression{"[\r\n]"}, QString::SkipEmptyParts))
        {
            auto terms = line.split(',', QString::KeepEmptyParts);
            if (terms.size() != 3)
            {
                throw std::runtime_error{fmt::format(
                    "Could not determine available networks - unexpected powershell output: {}", ps_output)};
            }

            auto links = switch_links(adapters, terms.at(2));
            ret.push_back({terms.at(0).toStdString(), "switch", switch_description(terms.at(1), links), links});
        }

        return ret;
    }

    auto detail = ps_output.isEmpty() ? "" : fmt::format(" Detail: {}", ps_output);
    auto err = fmt::format("Could not determine available networks - error executing powershell command.{}", detail);
    throw std::runtime_error{err};
}

auto mp::HyperVVirtualMachineFactory::get_adapters() -> std::vector<NetworkInterfaceInfo>
{
    std::vector<mp::NetworkInterfaceInfo> ret;
    for (auto& item : MP_PLATFORM.get_network_interfaces_info())
    {
        auto& net = item.second;
        if (const auto& type = net.type; type == "ethernet" || type == "wifi")
        {
            net.needs_authorization = true;
            ret.emplace_back(std::move(net));
        }
    }

    return ret;
}
