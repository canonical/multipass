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

#include "hyperv_driver_transition.h"
#include "daemon_config.h"
#include "hyperv_migration.h"
#include "instance_settings_handler.h"

#include <hyperv_api/hcs_virtual_machine_factory.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>

#include <multipass/constants.h>
#include <multipass/settings/settings.h>

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace mp = multipass;
namespace mhv = multipass::hyperv;

mhv::DriverTransition::DriverTransition(const DaemonConfig& config,
                                        const std::unordered_map<std::string, VMSpecs>& specs,
                                        const InstanceTable& operative_instances,
                                        const InstanceTable& deleted_instances,
                                        std::atomic<bool>& migration_in_progress,
                                        const std::atomic_size_t& preparations_in_progress)
    : config{config},
      specs{specs},
      operative_instances{operative_instances},
      deleted_instances{deleted_instances},
      migration_in_progress{migration_in_progress},
      preparations_in_progress{preparations_in_progress}
{
}

mhv::DriverTransition::~DriverTransition()
{
    if (migration_flag_acquired)
        migration_in_progress = false;
}

grpc::Status mhv::DriverTransition::prepare(const std::string& key, const std::string& value)
{
    if (key != mp::driver_key)
        return grpc::Status::OK;

    const auto current_driver = MP_SETTINGS.get(mp::driver_key).toStdString();
    const auto migrate_hyperv = current_driver == "hyperv" && value == "hyperv_api";
    const auto leave_hyperv_api = current_driver == "hyperv_api" && value != "hyperv_api";
    if (!migrate_hyperv && !leave_hyperv_api)
        return grpc::Status::OK;

    if (migrate_hyperv)
    {
        check_hyperv_api_support();
        migration_records = std::make_unique<HyperVMigrationTargetRecords>(config.data_directory);
        migration_records->preflight();
    }

    if (migration_in_progress.exchange(true))
        return migration_conflict_status("change settings");

    migration_flag_acquired = true;
    if (preparations_in_progress.load() != 0)
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "Cannot change driver while an instance is being prepared"};

    if (migrate_hyperv)
        migration_records->prepare();
    else
        release_hcs_instances();

    return grpc::Status::OK;
}

void mhv::DriverTransition::release_hcs_instances() const
{
    for (const auto* instances : {&operative_instances, &deleted_instances})
    {
        for (const auto& [name, vm] : *instances)
        {
            if (!vm)
                continue;

            const auto state = vm->current_state();
            if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
                throw mp::InstanceStateSettingsException{"Cannot change driver",
                                                         name,
                                                         "instance is not stopped"};
        }
    }

    for (const auto& [name, spec] : specs)
    {
        std::vector<std::string> mac_addresses{spec.default_mac_address};
        std::ranges::transform(spec.extra_interfaces,
                               std::back_inserter(mac_addresses),
                               &NetworkInterface::mac_address);
        if (!release_hcs_resources(name, mac_addresses))
            throw std::runtime_error{
                fmt::format("Could not release hyperv_api resources for '{}'", name)};
    }
}

grpc::Status mhv::DriverTransition::complete(
    grpc::ServerReaderWriterInterface<SetReply, SetRequest>* server)
{
    if (!migration_records)
        return grpc::Status::OK;

    DaemonHyperVInstanceMigrator migrator{specs,
                                          operative_instances,
                                          deleted_instances,
                                          *config.factory,
                                          *config.az_manager,
                                          config.data_directory,
                                          *migration_records};
    bool connection_lost = false;
    const MigrationReporter report = [server, &connection_lost](MigrationMessage kind,
                                                                const std::string& text) {
        SetReply reply;
        switch (kind)
        {
        case MigrationMessage::phase:
            reply.set_migration_phase(text);
            break;
        case MigrationMessage::diagnostic:
            reply.set_log_line(text);
            break;
        case MigrationMessage::summary:
            reply.set_summary(text);
            break;
        }

        if (!server->Write(reply))
            connection_lost = true;
    };
    const auto outcome = run_bulk_migration(migrator, report, [&connection_lost] {
        return connection_lost;
    });

    switch (outcome)
    {
    case MigrationOutcome::completed:
        return grpc::Status::OK;
    case MigrationOutcome::completed_with_failures:
        return {grpc::StatusCode::FAILED_PRECONDITION, "One or more instances failed to migrate"};
    case MigrationOutcome::cancelled:
        return {grpc::StatusCode::CANCELLED, "Hyper-V migration was cancelled"};
    case MigrationOutcome::aborted:
        return {grpc::StatusCode::FAILED_PRECONDITION, "Hyper-V migration aborted"};
    }
    throw std::logic_error{"Unknown Hyper-V migration outcome"};
}
