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

#include <hyperv_api/hcs_virtual_machine_factory.h>

#include <hyperv_api/hcn/hyperv_hcn_api_wrapper.h>
#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcs/hyperv_hcs_api_wrapper.h>
#include <hyperv_api/hcs_virtual_machine.h>
#include <hyperv_api/hcs_virtual_machine_exceptions.h>
#include <hyperv_api/virtdisk/virtdisk_api_wrapper.h>

#include <multipass/constants.h>
#include <multipass/platform.h>
#include <multipass/utils.h>
#include <multipass/vm_specs.h>

#include <computenetwork.h>

#include <fmt/std.h> // for the std::filesystem::path formatter

#include <regex>

namespace multipass::hyperv
{

/**
 * Category for the log messages.
 */
constexpr static auto kLogCategory = "HyperV-Virtual-Machine-Factory";
constexpr static auto kDefaultHyperVSwitchGUID = "C08CB7B8-9B3C-408E-8E30-5E16A3AEB444";
constexpr static auto kExtraInterfaceBridgeNameFmtStr = "Multipass Bridge ({})";
/**
 * Regex pattern to extract the origin network name and GUID from an extra interface
 * name.
 */
constexpr static auto kExtraInterfaceBridgeNameRegex = "Multipass Bridge \\((.*)\\)";

// Delegating constructor
HCSVirtualMachineFactory::HCSVirtualMachineFactory(const Path& data_dir)
    : HCSVirtualMachineFactory(data_dir,
                               std::make_shared<hcs::HCSWrapper>(),
                               std::make_shared<hcn::HCNWrapper>(),
                               std::make_shared<virtdisk::VirtDiskWrapper>())

{
}

HCSVirtualMachineFactory::HCSVirtualMachineFactory(const Path& data_dir,
                                                   hcs_sptr_t hcs,
                                                   hcn_sptr_t hcn,
                                                   virtdisk_sptr_t virtdisk)
    : BaseVirtualMachineFactory(
          MP_UTILS.derive_instances_dir(data_dir, get_backend_directory_name(), instances_subdir)),
      hcs_sptr(hcs),
      hcn_sptr(hcn),
      virtdisk_sptr(virtdisk)
{
    const std::array<void*, 3> api_ptrs = {hcs.get(), hcn.get(), virtdisk.get()};
    if (std::any_of(std::begin(api_ptrs), std::end(api_ptrs), [](const void* ptr) {
            return nullptr == ptr;
        }))
    {
        throw InvalidAPIPointerException{"One of the required API pointers is not set: {}.",
                                         fmt::join(api_ptrs, ",")};
    }
}

VirtualMachine::UPtr HCSVirtualMachineFactory::create_virtual_machine(
    const VirtualMachineDescription& desc,
    const SSHKeyProvider& key_provider,
    VMStatusMonitor& monitor)
{
    assert(hcs_sptr);
    assert(hcn_sptr);
    assert(virtdisk_sptr);

    const auto networks = MP_PLATFORM.get_network_interfaces_info();
    for (const auto& extra : desc.extra_interfaces)
    {
        std::regex pattern{kExtraInterfaceBridgeNameRegex};
        std::smatch match;

        // The origin interface name is encoded into the interface name itself.
        if (!std::regex_match(extra.id, match, pattern) || match.size() != 2)
        {
            mpl::error(kLogCategory, "Invalid extra interface name `{}`.", extra.id);
            continue;
        }

        const auto origin_interface_name = match[1].str();
        const auto origin_network_guid = match[2].str();

        const auto found = std::find_if(networks.begin(), networks.end(), [&](const auto& kvp) {
            const auto& [k, v] = kvp;
            return v.id == origin_interface_name;
        });

        if (networks.end() == found)
        {
            mpl::warn(kLogCategory,
                      "Could not find the source interface `{}` for extra `{}`",
                      origin_interface_name,
                      extra.id);
            continue;
        }

        create_bridge_with(found->second);
    }

    return std::make_unique<HCSVirtualMachine>(hcs_sptr,
                                               hcn_sptr,
                                               virtdisk_sptr,
                                               kDefaultHyperVSwitchGUID,
                                               desc,
                                               monitor,
                                               key_provider,
                                               get_instance_directory(desc.vm_name));
}

void HCSVirtualMachineFactory::remove_resources_for_impl(const std::string& name)
{
    mpl::debug(kLogCategory, "remove_resources_for_impl() -> VM: {}", name);
    // Everything for the VM is neatly packed into the VM folder, so it's enough to ensure that
    // the VM is stopped. The base class will take care of the nuking the VM folder.
    const auto& [status, status_msg] = hcs_sptr->terminate_compute_system(name);
    if (status)
    {
        mpl::warn(kLogCategory,
                  "remove_resources_for_impl() -> Host compute system {} was still alive.",
                  name);
    }
}

VMImage HCSVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
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
        throw ImageConversionException{
            "Conversion of image {} to VHDX failed with following error: {}",
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

void HCSVirtualMachineFactory::prepare_instance_image(const VMImage& instance_image,
                                                      const VirtualMachineDescription& desc)
{
    // Resize the instance image to the desired size
    assert(virtdisk_sptr);
    const auto& [status, status_msg] =
        virtdisk_sptr->resize_virtual_disk(instance_image.image_path.toStdString(),
                                           desc.disk_space.in_bytes());
    if (!status)
    {
        throw ImageResizeException{"Failed to resize VHDX file `{}`, virtdisk API error code `{}`",
                                   instance_image.image_path.toStdString(),
                                   status};
    }
}

std::string HCSVirtualMachineFactory::create_bridge_with(const NetworkInterfaceInfo& intf)
{
    const auto bridge_name = fmt::format(kExtraInterfaceBridgeNameFmtStr, intf.id);
    const auto params = [&intf, &bridge_name] {
        hcn::CreateNetworkParameters network_params{};
        network_params.name = bridge_name;
        network_params.type = hcn::HcnNetworkType::Transparent();
        network_params.guid = multipass::utils::make_uuid(network_params.name).toStdString();
        hcn::HcnNetworkPolicy policy{hcn::HcnNetworkPolicyType::NetAdapterName(),
                                     hcn::HcnNetworkPolicyNetAdapterName{intf.id}};
        network_params.policies.push_back(policy);
        return network_params;
    }();

    assert(hcn_sptr);
    const auto& [status, status_msg] = hcn_sptr->create_network(params);

    if (status || static_cast<HRESULT>(status) == HCN_E_NETWORK_ALREADY_EXISTS)
    {
        return params.name;
    }

    return {};
}

VirtualMachine::UPtr HCSVirtualMachineFactory::clone_vm_impl(const std::string& source_vm_name,
                                                             const multipass::VMSpecs& src_vm_specs,
                                                             const VirtualMachineDescription& desc,
                                                             VMStatusMonitor& monitor,
                                                             const SSHKeyProvider& key_provider)
{

    const fs::path src_vm_instance_dir{get_instance_directory(source_vm_name).toStdWString()};

    if (!fs::exists(src_vm_instance_dir))
    {
        throw std::runtime_error{"Source VM instance directory is missing!"};
    }

    std::optional<fs::path> src_vm_vhdx{std::nullopt};

    for (const auto& entry : fs::directory_iterator(src_vm_instance_dir))
    {
        const auto& extension = entry.path().extension();
        if (extension == ".vhdx")
        {
            src_vm_vhdx = entry.path();
            break;
        }
    }

    if (!src_vm_vhdx.has_value())
    {
        throw std::runtime_error{"Could not locate source VM's vhdx file!"};
    }

    // Copy the VHDX file.
    virtdisk::CreateVirtualDiskParameters clone_vhdx_params{};
    clone_vhdx_params.predecessor = virtdisk::SourcePathParameters{src_vm_vhdx.value()};
    clone_vhdx_params.path = desc.image.image_path.toStdString();
    clone_vhdx_params.size_in_bytes = 0; // use source disk size

    const auto& [status, msg] = virtdisk_sptr->create_virtual_disk(clone_vhdx_params);

    if (!status)
    {
        throw std::runtime_error{"VHDX clone failed."};
    }

    return create_virtual_machine(desc, key_provider, monitor);
}

std::vector<NetworkInterfaceInfo> HCSVirtualMachineFactory::get_adapters()
{
    std::vector<NetworkInterfaceInfo> ret;
    for (auto& item : MP_PLATFORM.get_network_interfaces_info())
    {
        auto& net = item.second;
        if (const auto& type = net.type; type == "Ethernet")
        {
            net.needs_authorization = true;
            ret.emplace_back(std::move(net));
        }
    }

    return ret;
}

std::vector<NetworkInterfaceInfo> HCSVirtualMachineFactory::networks() const
{
    return get_adapters();
}

} // namespace multipass::hyperv
