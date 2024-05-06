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

#ifndef MULTIPASS_MOCK_VIRTUAL_MACHINE_FACTORY_H
#define MULTIPASS_MOCK_VIRTUAL_MACHINE_FACTORY_H

#include "common.h"

#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_status_monitor.h>

namespace multipass
{
namespace test
{
struct MockVirtualMachineFactory : public VirtualMachineFactory
{
    MOCK_METHOD(VirtualMachine::UPtr,
                create_virtual_machine,
                (const VirtualMachineDescription&, const SSHKeyProvider&, VMStatusMonitor&),
                (override));
    MOCK_METHOD(VirtualMachine::UPtr,
                create_vm_and_instance_disk_data,
                (const VMSpecs&,
                 const VMSpecs&,
                 const std::string&,
                 const std::string&,
                 const VMImage&,
                 const SSHKeyProvider&,
                 VMStatusMonitor&),
                (override));
    MOCK_METHOD(void, remove_resources_for, (const std::string&), (override));

    MOCK_METHOD(FetchType, fetch_type, (), (override));
    MOCK_METHOD(void, prepare_networking, (std::vector<NetworkInterface>&), (override));
    MOCK_METHOD(VMImage, prepare_source_image, (const VMImage&), (override));
    MOCK_METHOD(void, prepare_instance_image, (const VMImage&, const VirtualMachineDescription&), (override));
    MOCK_METHOD(void, hypervisor_health_check, (), (override));
    MOCK_METHOD(QString, get_backend_directory_name, (), (const, override));
    MOCK_METHOD(QString, get_instance_directory, (const std::string&), (const, override));
    MOCK_METHOD(QString, get_backend_version_string, (), (const, override));
    MOCK_METHOD(VMImageVault::UPtr, create_image_vault,
                (std::vector<VMImageHost*>, URLDownloader*, const Path&, const Path&, const days&), (override));
    MOCK_METHOD(void, configure, (VirtualMachineDescription&), (override));
    MOCK_METHOD(std::vector<NetworkInterfaceInfo>, networks, (), (const, override));
    MOCK_METHOD(void, require_snapshots_support, (), (const, override));
    MOCK_METHOD(void, require_suspend_support, (), (const, override));
    MOCK_METHOD(void, require_clone_support, (), (const, override));

    // originally protected:
    MOCK_METHOD(std::string, create_bridge_with, (const NetworkInterfaceInfo&), (override));
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_VIRTUAL_MACHINE_FACTORY_H
