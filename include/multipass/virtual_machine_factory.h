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

#ifndef MULTIPASS_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_VIRTUAL_MACHINE_FACTORY_H

#include "days.h"
#include "disabled_copy_move.h"
#include "fetch_type.h"
#include "path.h"
#include "virtual_machine.h"
#include "vm_image.h"
#include "vm_image_vault.h"

namespace YAML
{
class Node;
}

namespace multipass
{
class SSHKeyProvider;
class URLDownloader;
class VirtualMachineDescription;
class VMImageHost;
class VMStatusMonitor;
struct NetworkInterface;
struct NetworkInterfaceInfo;

class VirtualMachineFactory : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<VirtualMachineFactory>;
    virtual ~VirtualMachineFactory() = default;
    virtual VirtualMachine::UPtr create_virtual_machine(const VirtualMachineDescription& desc,
                                                        const SSHKeyProvider& key_provider,
                                                        VMStatusMonitor& monitor) = 0;
    virtual VirtualMachine::UPtr create_vm_and_instance_disk_data(const QString& data_directory,
                                                                  const VMSpecs& src_vm_spec,
                                                                  const VMSpecs& dest_vm_spec,
                                                                  const std::string& source_name,
                                                                  const std::string& destination_name,
                                                                  const VMImage& dest_vm_image,
                                                                  const SSHKeyProvider& key_provider,
                                                                  VMStatusMonitor& monitor) = 0;

    /** Removes any resources associated with a VM of the given name.
     *
     * @param name The unique name assigned to the virtual machine
     */
    virtual void remove_resources_for(const std::string& name) = 0;

    virtual FetchType fetch_type() = 0;
    virtual void prepare_networking(std::vector<NetworkInterface>& extra_interfaces) = 0; // note the arg may be updated
    virtual VMImage prepare_source_image(const VMImage& source_image) = 0;
    virtual void prepare_instance_image(const VMImage& instance_image, const VirtualMachineDescription& desc) = 0;
    virtual void hypervisor_health_check() = 0;
    virtual QString get_backend_directory_name() const = 0;
    virtual Path get_instance_directory(const std::string& name) const = 0;
    virtual QString get_backend_version_string() const = 0;
    virtual VMImageVault::UPtr create_image_vault(std::vector<VMImageHost*> image_hosts, URLDownloader* downloader,
                                                  const Path& cache_dir_path, const Path& data_dir_path,
                                                  const days& days_to_expire) = 0;
    virtual void configure(VirtualMachineDescription& vm_desc) = 0;

    // List all the network interfaces seen by the backend.
    virtual std::vector<NetworkInterfaceInfo> networks() const = 0;
    virtual void require_snapshots_support() const = 0;
    virtual void require_suspend_support() const = 0;
    virtual void require_clone_support() const = 0;

protected:
    VirtualMachineFactory() = default;

    virtual std::string create_bridge_with(const NetworkInterfaceInfo& interface) = 0;
};
}
#endif // MULTIPASS_VIRTUAL_MACHINE_FACTORY_H
