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
#include "hyperv_migration_state.h"

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
#include <shared/windows/powershell.h>

#include <QUuid>

#include <fmt/format.h>
#include <fmt/std.h>
#include <scope_guard.hpp>

#include <algorithm>
#include <cctype>
#include <system_error>

namespace
{
namespace fs = std::filesystem;
namespace mhv = multipass::hyperv;
namespace mpl = multipass::logging;
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
                        std::string hostname)
        : HCSVirtualMachine{mhv::default_hyperv_switch_guid,
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

std::string normalized_mac(std::string mac)
{
    std::erase_if(mac, [](char c) { return c == ':' || c == '-'; });
    std::ranges::transform(mac, mac.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return mac;
}

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

void verify_trial(TrialVirtualMachine& trial,
                  const multipass::VirtualMachineDescription& description,
                  const multipass::Path& instance_dir)
{
    const auto cloud_init_path =
        fs::path{instance_dir.toStdString()} / multipass::cloud_init_file_name;
    const auto expected_instance_id =
        MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(cloud_init_path);
    const auto actual_instance_id =
        multipass::utils::trim(trial.ssh_exec("cat /var/lib/cloud/data/instance-id"));
    if (actual_instance_id != expected_instance_id)
        throw std::runtime_error{
            fmt::format("Migration trial booted an unexpected cloud-init instance: expected '{}', "
                        "received '{}'",
                        expected_instance_id,
                        actual_instance_id)};

    const auto actual_mac = multipass::utils::trim(trial.ssh_exec(
        "iface=$(ip route show default | awk 'NR==1 {print $5}'); "
        "cat /sys/class/net/$iface/address"));
    if (normalized_mac(actual_mac) != normalized_mac(description.default_mac_address))
        throw std::runtime_error{
            fmt::format("Migration trial primary MAC mismatch: expected '{}', received '{}'",
                        description.default_mac_address,
                        actual_mac)};
}

void run_trial(const mhv::LegacyHyperVDiskLayout& layout,
               const multipass::VirtualMachineDescription& description,
               const multipass::SSHKeyProvider& key_provider,
               multipass::AvailabilityZone& zone,
               const multipass::Path& instance_dir)
{
    const auto suffix =
        QUuid::createUuid().toString(QUuid::WithoutBraces).left(8).toStdString();
    const auto trial_name = fmt::format("{}-migration-{}", description.vm_name, suffix);
    auto trial_disk = layout.active_disk;
    trial_disk += fmt::format(".{}.trial.avhdx", suffix);
    auto state_file_stem = trial_disk;
    state_file_stem += ".state";

    const mhv::virtdisk::CreateVirtualDiskParameters trial_disk_params{
        .path = trial_disk,
        .predecessor = mhv::virtdisk::ParentPathParameters{layout.active_disk}};
    if (const auto result = VirtDisk().create_virtual_disk(trial_disk_params); !result)
        throw std::runtime_error{
            fmt::format("Could not create HCS migration trial disk '{}': {}", trial_disk, result)};

    std::unique_ptr<TrialVirtualMachine> trial_vm;
    auto cleanup = sg::make_scope_guard([&]() noexcept {
        multipass::top_catch_all(log_category, [&] { mhv::remove_hcs_resources(trial_name); });
        trial_vm.reset();
        multipass::top_catch_all(
            log_category,
            [&] { remove_trial_files(trial_disk, state_file_stem); });
    });

    auto trial_description = description;
    trial_description.vm_name = trial_name;
    trial_description.image.image_path = trial_disk;

    TrialMonitor monitor;
    trial_vm = std::make_unique<TrialVirtualMachine>(
        trial_description,
        monitor,
        key_provider,
        zone,
        instance_dir,
        state_file_stem,
        fmt::format("{}.mshome.net", description.vm_name));
    trial_vm->start();
    trial_vm->wait_until_ssh_up(multipass::default_timeout);
    verify_trial(*trial_vm, description, instance_dir);

    mhv::remove_hcs_resources(trial_name);
    trial_vm.reset();
    remove_trial_files(trial_disk, state_file_stem);
    cleanup.dismiss();
}

std::optional<std::string>
remove_legacy_registration(const std::string& name,
                           const mhv::LegacyHyperVDiskLayout& layout)
{
    struct HeldDisk
    {
        fs::path original;
        fs::path holding;
        bool held{false};
    };

    std::vector<HeldDisk> disks;
    disks.reserve(layout.all_disks.size());
    for (const auto& disk : layout.all_disks)
    {
        auto holding = disk;
        holding += ".multipass-migration-hold";
        if (MP_FILEOPS.exists(holding))
            throw std::runtime_error{
                fmt::format("Migration holding path already exists: '{}'", holding)};
        disks.push_back({.original = disk, .holding = std::move(holding)});
    }

    auto restore = sg::make_scope_guard([&]() noexcept {
        for (auto it = disks.rbegin(); it != disks.rend(); ++it)
            if (it->held)
                multipass::top_catch_all(
                    log_category,
                    [&disk = *it] { MP_FILEOPS.rename(disk.holding, disk.original); });
    });

    for (auto& disk : disks)
    {
        MP_FILEOPS.rename(disk.original, disk.holding);
        disk.held = true;
    }

    QString output_error;
    if (!multipass::PowerShell::exec({"-NoProfile",
                                      "-NonInteractive",
                                      "-Command",
                                      "Remove-VM",
                                      "-Name",
                                      QString::fromStdString(name),
                                      "-Force"},
                                     name,
                                     nullptr,
                                     &output_error))
    {
        if (!mhv::HyperVDiskLayoutResolver::vm_exists(name))
            return fmt::format(
                "Legacy registration disappeared while Remove-VM reported failure: {}",
                output_error.toStdString());

        throw std::runtime_error{
            fmt::format("Could not remove legacy Hyper-V registration: {}",
                        output_error.toStdString())};
    }

    try
    {
        for (auto it = disks.rbegin(); it != disks.rend(); ++it)
        {
            MP_FILEOPS.rename(it->holding, it->original);
            it->held = false;
        }
    }
    catch (const std::exception& e)
    {
        return fmt::format("Legacy registration was removed but disk restoration failed: {}",
                           e.what());
    }

    restore.dismiss();
    return std::nullopt;
}
} // namespace

multipass::hyperv::DefaultHyperVMigrator::DefaultHyperVMigrator(
    VirtualMachineDescription description,
    VMStatusMonitor& monitor,
    const SSHKeyProvider& key_provider,
    AvailabilityZone& zone,
    Path instance_dir)
    : description{std::move(description)},
      monitor{monitor},
      key_provider{key_provider},
      zone{zone},
      instance_dir{std::move(instance_dir)}
{
}

multipass::VirtualMachine::UPtr
multipass::hyperv::DefaultHyperVMigrator::make_target()
{
    if (!committed_state)
        throw std::runtime_error{"Hyper-V migration has not been committed"};

    auto hcs_description = description;
    hcs_description.image.image_path = committed_state->active_disk;
    auto target = std::make_unique<HCSVirtualMachine>(default_hyperv_switch_guid,
                                                      hcs_description,
                                                      monitor,
                                                      key_provider,
                                                      zone,
                                                      instance_dir,
                                                      committed_state->hcs_state_file_stem);
    target->load_snapshots();
    return target;
}

void multipass::hyperv::DefaultHyperVMigrator::commit_migration(
    const LegacyHyperVDiskLayout& layout,
    HyperVMigrationState state)
{
    state.backend = HyperVBackend::hcs;
    state.persist(instance_dir.toStdString());
    committed_state = state;

    try
    {
        if (const auto error = remove_legacy_registration(description.vm_name, layout))
            mpl::error(
                description.vm_name,
                "Hyper-V migration crossed the commit boundary but did not finish cleanly: {}",
                *error);
    }
    catch (...)
    {
        committed_state.reset();
        state.backend = HyperVBackend::legacy;
        state.persist(instance_dir.toStdString());
        throw;
    }
}

bool
multipass::hyperv::DefaultHyperVMigrator::try_migrate(VirtualMachine& legacy_vm)
{
    if (committed_state)
        return true;

    try
    {
        const auto layout = HyperVDiskLayoutResolver::resolve(description.vm_name, legacy_vm);
        HyperVMigrationState state{
            .backend = HyperVBackend::legacy,
            .active_disk = layout.active_disk,
            .hcs_state_file_stem =
                fs::path{instance_dir.toStdString()} / "hcs-migrated-state",
            .snapshots = layout.snapshots,
        };
        state.persist_snapshot_paths(legacy_vm);
        state.persist(instance_dir.toStdString());
        run_trial(layout, description, key_provider, zone, instance_dir);
        commit_migration(layout, std::move(state));
    }
    catch (const std::exception& e)
    {
        mpl::warn(description.vm_name,
                  "Hyper-V migration was not committed; continuing with the legacy backend: {}",
                  e.what());
        return false;
    }

    return true;
}
