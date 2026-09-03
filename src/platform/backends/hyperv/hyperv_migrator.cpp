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

#include "hyperv_migrator.h"

#include "hyperv_disk_layout.h"
#include "hyperv_target_transaction.h"

#include <hyperv_api/hcs_virtual_machine.h>
#include <hyperv_api/hcs_virtual_machine_factory.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#include <hyperv_api/virtdisk/virtdisk_create_virtual_disk_params.h>
#include <hyperv_api/virtdisk/virtdisk_wrapper.h>

#include <multipass/cloud_init_iso.h>
#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/logging/log.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <multipass/vm_status_monitor.h>

#include <QUuid>

#include <fmt/format.h>
#include <fmt/std.h>
#include <scope_guard.hpp>

#include <system_error>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;
using multipass::hyperv::virtdisk::VirtDisk;

constexpr auto log_category = "Hyper-V migration";

class TrialMonitor final : public multipass::VMStatusMonitor
{
public:
    void on_resume() override
    {
    }
    void on_shutdown() override
    {
    }
    void on_suspend() override
    {
    }
    void on_restart(const std::string&) override
    {
    }
    void persist_state_for(const std::string&, const multipass::VirtualMachine::State&) override
    {
    }
    void update_metadata_for(const std::string&, const boost::json::object&) override
    {
    }
    boost::json::object retrieve_metadata_for(const std::string&) override
    {
        return {};
    }
};

class TrialVirtualMachine final : public mhv::HCSVirtualMachine
{
public:
    TrialVirtualMachine(const multipass::VirtualMachineDescription& description,
                        multipass::VMStatusMonitor& monitor,
                        const multipass::SSHKeyProvider& key_provider,
                        multipass::AvailabilityZone& zone,
                        const multipass::Path& instance_dir,
                        const fs::path& state_file_stem,
                        const std::string& primary_network_guid,
                        std::string hostname)
        : HCSVirtualMachine{primary_network_guid,
                            description,
                            monitor,
                            key_provider,
                            zone,
                            instance_dir,
                            state_file_stem},
          hostname{std::move(hostname)}
    {
    }

    std::string ssh_hostname() override
    {
        return hostname;
    }

private:
    const std::string hostname;
};

fs::path with_extension(fs::path path, const char* extension)
{
    return path.replace_extension(extension);
}

void remove_trial_file(const fs::path& path)
{
    std::error_code error;
    if (MP_FILEOPS.exists(path, error) && !MP_FILEOPS.remove(path, error))
        throw std::runtime_error{
            fmt::format("Could not remove migration trial file '{}': {}", path, error.message())};
}

void remove_trial_files(const fs::path& trial_disk, const fs::path& state_file_stem)
{
    remove_trial_file(trial_disk);
    remove_trial_file(with_extension(state_file_stem, ".vmgs"));
    remove_trial_file(with_extension(state_file_stem, ".vmrs"));
    remove_trial_file(with_extension(state_file_stem, ".SavedState.vmrs"));
}

std::vector<std::string> observed_trial_macs(TrialVirtualMachine& trial)
{
    const auto output = trial.ssh_exec(
        "for iface in /sys/class/net/*; do "
        "[ \"$(basename \"$(readlink -f \"$iface/device/driver\")\")\" = hv_netvsc ] || continue; "
        "cat \"$iface/address\"; done");
    return multipass::utils::split(output, "\n");
}

void verify_trial(TrialVirtualMachine& trial,
                  const multipass::VirtualMachineDescription& description,
                  const fs::path& cloud_init_path)
{
    const auto expected_instance_id = MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(
        cloud_init_path);
    const auto actual_instance_id = multipass::utils::trim(
        trial.ssh_exec("cat /var/lib/cloud/data/instance-id"));
    if (actual_instance_id != expected_instance_id)
        throw std::runtime_error{
            fmt::format("Migration trial booted an unexpected cloud-init instance: expected '{}', "
                        "received '{}'",
                        expected_instance_id,
                        actual_instance_id)};

    mhv::verify_trial_macs(description, observed_trial_macs(trial));
}

// Trial-boots only the active target head through a disposable, target-local differencing
// disk. The target copies are never mutated by the trial.
void run_trial(const fs::path& target_head,
               const fs::path& staging_dir,
               const multipass::VirtualMachineDescription& description,
               const multipass::SSHKeyProvider& key_provider,
               multipass::AvailabilityZone& zone,
               const std::string& primary_network_guid,
               const fs::path& cloud_init_path)
{
    const auto suffix = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toStdString();
    const auto trial_name = fmt::format("{}-migration-{}", description.vm_name, suffix);
    auto trial_disk = staging_dir / fmt::format("{}.trial.avhdx", suffix);
    auto state_file_stem = trial_disk;
    state_file_stem += ".state";

    const mhv::virtdisk::CreateVirtualDiskParameters trial_disk_params{
        .path = trial_disk,
        .predecessor = mhv::virtdisk::ParentPathParameters{target_head}};
    if (const auto result = VirtDisk().create_virtual_disk(trial_disk_params); !result)
        throw std::runtime_error{
            fmt::format("Could not create HCS migration trial disk '{}': {}", trial_disk, result)};

    std::unique_ptr<TrialVirtualMachine> trial_vm;
    const auto cleanup_trial = [&] {
        (void)mhv::release_hcs_resources(trial_name);
        trial_vm.reset();
        remove_trial_files(trial_disk, state_file_stem);
    };
    auto cleanup = sg::make_scope_guard(
        [&]() noexcept { multipass::top_catch_all(log_category, cleanup_trial); });

    auto trial_description = description;
    trial_description.vm_name = trial_name;
    trial_description.image.image_path = trial_disk;
    trial_description.cloud_init_iso = QString::fromStdWString(cloud_init_path.wstring());

    TrialMonitor monitor;
    const auto staging_path = QString::fromStdWString(staging_dir.wstring());
    trial_vm = std::make_unique<TrialVirtualMachine>(
        trial_description,
        monitor,
        key_provider,
        zone,
        staging_path,
        state_file_stem,
        primary_network_guid,
        fmt::format("{}.mshome.net", description.vm_name));
    trial_vm->start();
    trial_vm->wait_until_ssh_up(multipass::default_timeout);
    verify_trial(*trial_vm, description, cloud_init_path);

    cleanup_trial();
    cleanup.dismiss();
}

void report_phase(const multipass::hyperv::MigrationPhaseCallback& on_phase, std::string_view phase)
{
    if (on_phase)
        on_phase(phase);
}
} // namespace

multipass::hyperv::HCSOwnership multipass::hyperv::migrate_retained_copy(
    const LegacyDiskLayout& layout,
    const fs::path& source_instance_dir,
    const VirtualMachineDescription& description,
    const fs::path& target_instance_dir,
    const SSHKeyProvider& key_provider,
    AvailabilityZone& zone,
    const std::string& primary_network_guid,
    const MigrationPhaseCallback& on_phase)
{
    TargetMigrationTransaction transaction{description.vm_name, target_instance_dir};

    report_phase(on_phase, "copying disks to the target instance");
    const auto mapping = transaction.stage(layout, source_instance_dir);

    report_phase(on_phase, "verifying the copied disks");
    transaction.verify(mapping, layout);

    report_phase(on_phase, "trial-booting the migrated instance");
    run_trial(mapping.active_disk,
              transaction.staging_dir(),
              description,
              key_provider,
              zone,
              primary_network_guid,
              transaction.staging_dir() / multipass::cloud_init_file_name);

    report_phase(on_phase, "committing the migrated instance");
    return transaction.commit(mapping, layout, source_instance_dir);
}
