/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "default_vm_image_vault.h"

#include <hyperv/hyperv_migration_service.h>

#include <multipass/path.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_specs.h>

#include <boost/json.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace multipass
{
class AvailabilityZoneManager;
class SSHKeyProvider;
class VirtualMachineFactory;
} // namespace multipass

namespace multipass::hyperv
{
class HCSVirtualMachineFactory;

class HyperVMigrationTargetRecords
{
public:
    explicit HyperVMigrationTargetRecords(const Path& data_dir);

    void preflight() const;
    void prepare();

    [[nodiscard]] bool target_exists(const std::string& name) const;
    [[nodiscard]] VaultRecord source_image_record(const std::string& name) const;
    [[nodiscard]] std::filesystem::path instance_dir(const std::string& name) const;

    void commit(const std::string& name, const VMSpecs& spec, VaultRecord image_record);

private:
    static boost::json::object load_records(const std::filesystem::path& path);
    static void persist_records(const boost::json::object& records,
                                const std::filesystem::path& path);
    static void require_writable_location(const std::filesystem::path& path);
    void recover();
    [[nodiscard]] bool has_vm_record(const std::string& name) const;
    [[nodiscard]] bool has_image_record(const std::string& name) const;

    std::filesystem::path source_image_db;
    std::filesystem::path target_root;
    std::filesystem::path target_vm_db;
    std::filesystem::path target_image_db;
    std::filesystem::path instances_root;
    boost::json::object source_image_records;
    boost::json::object target_vm_records;
    boost::json::object target_image_records;
};

class DaemonHyperVInstanceMigrator final : public InstanceMigrator
{
public:
    using InstanceTable = std::unordered_map<std::string, VirtualMachine::ShPtr>;

    DaemonHyperVInstanceMigrator(const std::unordered_map<std::string, VMSpecs>& specs,
                                 const InstanceTable& operative_instances,
                                 const InstanceTable& deleted_instances,
                                 VirtualMachineFactory& source_factory,
                                 const SSHKeyProvider& key_provider,
                                 AvailabilityZoneManager& az_manager,
                                 const Path& data_dir,
                                 HyperVMigrationTargetRecords& target_records);
    ~DaemonHyperVInstanceMigrator() override;

    [[nodiscard]] std::vector<std::string> source_names() override;
    [[nodiscard]] InstanceMigrationResult migrate(const std::string& name,
                                                  MigrationProgress& progress) override;

private:
    [[nodiscard]] std::vector<NetworkInterface> translated_interfaces(
        const std::vector<NetworkInterface>& source_interfaces,
        std::vector<std::string>& created_network_guids);
    static void cleanup_created_networks(const std::vector<std::string>& network_guids) noexcept;
    HCSVirtualMachineFactory& target_factory();

    const std::unordered_map<std::string, VMSpecs>& specs;
    const InstanceTable& operative_instances;
    const InstanceTable& deleted_instances;
    VirtualMachineFactory& source_factory;
    const SSHKeyProvider& key_provider;
    AvailabilityZoneManager& az_manager;
    Path data_dir;
    HyperVMigrationTargetRecords& target_records;
    std::optional<std::vector<NetworkInterfaceInfo>> source_networks;
    std::unique_ptr<HCSVirtualMachineFactory> hcs_factory;
};
} // namespace multipass::hyperv
