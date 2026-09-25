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

// TODO hyperv migration, remove (whole file)

#include "hyperv_driver_transition.h"
#include "daemon_config.h"
#include "hyperv_migration.h"

#include <hcs/hcs_virtual_machine_factory.h>
#include <hcs/hcs_virtual_machine_resources.h>

#include <multipass/constants.h>
#include <multipass/settings/settings.h>

#include <fmt/ranges.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace mp = multipass;
namespace mhv = multipass::hyperv;

mhv::DriverTransition::DriverTransition(DriverTransitionContext context) : context{context}
{
}

mhv::DriverTransition::~DriverTransition()
{
    if (migration_flag_acquired)
        context.migration_in_progress = false;
}

grpc::Status mhv::DriverTransition::prepare(const std::string& key, const std::string& value)
{
    if (key != mp::driver_key)
        return grpc::Status::OK;

    const auto current_driver = MP_SETTINGS.get(mp::driver_key).toStdString();
    const auto migrate_hyperv = current_driver == "hyperv" && value == "hcs";
    const auto leave_hcs = current_driver == "hcs" && value != "hcs";
    if (!migrate_hyperv && !leave_hcs)
        return grpc::Status::OK;

    if (migrate_hyperv)
    {
        check_hcs_support();
        migration_records = std::make_unique<HyperVMigrationTargetRecords>(
            context.config.data_directory);
        migration_records->preflight();
    }

    if (context.migration_in_progress.exchange(true))
        return migration_conflict_status("change settings");

    migration_flag_acquired = true;
    if (!context.preparing_instances.empty())
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "Cannot change driver while an instance is being prepared"};

    if (!migrate_hyperv)
        return release_hcs_instances();

    migration_records->prepare();
    return grpc::Status::OK;
}

grpc::Status mhv::DriverTransition::release_hcs_instances() const
{
    std::vector<std::string> not_stopped;
    for (const auto* instances : {&context.operative_instances, &context.deleted_instances})
    {
        for (const auto& [name, vm] : *instances)
        {
            if (!vm)
                continue;

            const auto state = vm->current_state();
            if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
                not_stopped.push_back(name);
        }
    }

    if (!not_stopped.empty())
    {
        std::ranges::sort(not_stopped);
        return {grpc::StatusCode::FAILED_PRECONDITION,
                fmt::format("{} must be stopped first", fmt::join(not_stopped, ", "))};
    }

    for (const auto& [name, spec] : context.specs)
    {
        if (!release_hcs_resources(name))
            throw std::runtime_error{fmt::format("Could not release HCS resources for '{}'", name)};
    }

    return grpc::Status::OK;
}

grpc::Status mhv::DriverTransition::complete(
    grpc::ServerReaderWriterInterface<SetReply, SetRequest>* server)
{
    if (!migration_records)
        return grpc::Status::OK;

    DaemonHyperVInstanceMigrator migrator{context.specs,
                                          context.operative_instances,
                                          context.deleted_instances,
                                          *context.config.factory,
                                          *context.config.az_manager,
                                          context.config.data_directory,
                                          *migration_records};
    bool connection_lost = false;
    const MigrationReporter report = [server, &connection_lost](MigrationMessage kind,
                                                                const std::string& text) {
        SetReply reply;
        switch (kind)
        {
        case MigrationMessage::phase:
            reply.mutable_hcs_migration_report()->set_phase(text);
            break;
        case MigrationMessage::diagnostic:
            reply.set_log_line(text);
            break;
        case MigrationMessage::summary:
            reply.mutable_hcs_migration_report()->set_summary(text);
            break;
        }

        if (!server->Write(reply))
            connection_lost = true;
    };
    const auto outcome = migrator.migrate_all(report,
                                              [&connection_lost] { return connection_lost; });

    switch (outcome)
    {
    case MigrationOutcome::completed:
        return grpc::Status::OK;
    case MigrationOutcome::completed_with_failures:
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "Driver change succeeded, but one or more instances failed to migrate"};
    case MigrationOutcome::cancelled:
        return {grpc::StatusCode::CANCELLED,
                "Driver change succeeded, but the migration was cancelled"};
    case MigrationOutcome::aborted:
        return {grpc::StatusCode::FAILED_PRECONDITION,
                "Driver change succeeded, but the migration was aborted"};
    }
    throw std::logic_error{"Unknown Hyper-V migration outcome"};
}
