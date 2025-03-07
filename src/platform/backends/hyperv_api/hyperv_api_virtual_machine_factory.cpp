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

#include "hyperv_api_virtual_machine_factory.h"
#include "hyperv_api_virtual_machine.h"
#include <multipass/constants.h>
#include <multipass/utils.h>

#include <computenetwork.h>

#include <fmt/std.h> // for the std::filesystem::path formatter

namespace multipass::hyperv
{

/**
 * Category for the log messages.
 */
static constexpr auto kLogCategory = "HyperV-Virtual-Machine-Factory";
static constexpr auto kDefaultHyperVSwitchGUID = "C08CB7B8-9B3C-408E-8E30-5E16A3AEB444";

struct ExceptionFormatter : std::runtime_error
{
    template <typename... Args>
    ExceptionFormatter(fmt::format_string<Args...> fmt, Args&&... args)
        : std::runtime_error{fmt::format(fmt, std::forward<Args>(args)...)}
    {
    }

    ExceptionFormatter() : std::runtime_error("")
    {
    }
};

struct ImageConversionException : public ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

struct ImageResizeException : public ExceptionFormatter
{
    using ExceptionFormatter::ExceptionFormatter;
};

HyperVAPIVirtualMachineFactory::HyperVAPIVirtualMachineFactory(const Path& data_dir)
    : BaseVirtualMachineFactory(MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir))
{
}

VirtualMachine::UPtr HyperVAPIVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                                            const SSHKeyProvider& key_provider,
                                                                            VMStatusMonitor& monitor)
{
    auto hcs = std::make_unique<hcs::HCSWrapper>();
    auto hcn = std::make_unique<hcn::HCNWrapper>();
    auto virtdisk = std::make_unique<virtdisk::VirtDiskWrapper>();

    return std::make_unique<HyperVAPIVirtualMachine>(std::move(hcs),
                                                     std::move(hcn),
                                                     std::move(virtdisk),
                                                     kDefaultHyperVSwitchGUID,
                                                     desc,
                                                     monitor,
                                                     key_provider,
                                                     get_instance_directory(desc.vm_name));
}

void HyperVAPIVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
    throw std::runtime_error{"Not implemented yet."};
}

VMImage HyperVAPIVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    const std::filesystem::path source_file{source_image.image_path.toStdString()};

    if (!std::filesystem::exists(source_file))
    {
        throw ImageConversionException{"Image {} does not exist", source_file};
    }

    const std::filesystem::path target_file = [source_file]() {
        auto target_file = source_file;
        target_file.replace_extension(".vhdx");
        return target_file;
    }();

    const QStringList qemu_img_args{"convert",
                                    "-o",
                                    "subformat=dynamic",
                                    "-O",
                                    "vhdx",
                                    QString::fromStdString(source_file.string()),
                                    QString::fromStdString(target_file.string())};

    QProcess qemu_img_process{};
    qemu_img_process.setProgram("qemu-img.exe");
    qemu_img_process.setArguments(qemu_img_args);
    qemu_img_process.start();

    if (!qemu_img_process.waitForFinished(multipass::image_resize_timeout))
    {
        throw ImageConversionException{"Conversion of image {} to VHDX timed out", source_file};
    }

    if (qemu_img_process.exitCode() != 0)
    {
        throw ImageConversionException{"Conversion of image {} to VHDX failed with following error: {}",
                                       source_file,
                                       qemu_img_process.readAllStandardError().toStdString()};
    }

    if (!std::filesystem::exists(target_file))
    {
        throw ImageConversionException{"Converted VHDX `{}` does not exist!", target_file};
    }

    VMImage result{source_image};
    result.image_path = QString::fromStdString(target_file.string());
    return result;
}

void HyperVAPIVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                            const VirtualMachineDescription& desc)
{
    // FIXME:
    virtdisk::VirtDiskWrapper wrap{};

    // Resize the instance image to the desired size
    const auto& [status, status_msg] =
        wrap.resize_virtual_disk(instance_image.image_path.toStdString(), desc.disk_space.in_bytes());
    if (!status)
    {
        throw ImageResizeException{"Failed to resize VHDX file `{}`, virtdisk API error code `{}`",
                                   instance_image.image_path.toStdString(),
                                   status};
    }
}

std::string HyperVAPIVirtualMachineFactory::create_bridge_with(const NetworkInterfaceInfo& intf)
{
    (void)intf;
    // No-op. The implementation uses the default Hyper-V switch.
    return {};
}

} // namespace multipass::hyperv
