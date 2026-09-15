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

#include <multipass/disabled_copy_move.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_specs.h>

#include <fmt/format.h>
#include <grpcpp/support/status.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace grpc
{
template <typename Write, typename Read>
class ServerReaderWriterInterface;
}

namespace multipass
{
struct DaemonConfig;
class SetRequest;
class SetReply;
} // namespace multipass

namespace multipass::hyperv
{
inline grpc::Status migration_conflict_status(std::string_view rpc_name)
{
    return {grpc::StatusCode::FAILED_PRECONDITION,
            fmt::format("Cannot {} while a Hyper-V instance migration is in progress. Please wait "
                        "for the migration to finish and try again.",
                        rpc_name)};
}

class HyperVMigrationTargetRecords;

using InstanceTable = std::unordered_map<std::string, VirtualMachine::ShPtr>;

// Bundles the slice of Daemon state that a driver transition needs to read or mutate, so it
// doesn't have to depend on (or be a friend of) the whole Daemon class.
struct DriverTransitionContext
{
    const DaemonConfig& config;
    const std::unordered_map<std::string, VMSpecs>& specs;
    const InstanceTable& operative_instances;
    const InstanceTable& deleted_instances;
    std::atomic<bool>& migration_in_progress;
    const std::atomic_size_t& preparations_in_progress;
};

class DriverTransition : private DisabledCopyMove
{
public:
    using InstanceTable = multipass::hyperv::InstanceTable;

    explicit DriverTransition(DriverTransitionContext context);
    ~DriverTransition();

    // Keep this object alive across the settings write and completion, including error exits.
    [[nodiscard]] grpc::Status prepare(const std::string& key, const std::string& value);
    [[nodiscard]] grpc::Status complete(
        grpc::ServerReaderWriterInterface<SetReply, SetRequest>* server);

private:
    void release_hcs_instances() const;

    DriverTransitionContext context;
    bool migration_flag_acquired{false};
    std::unique_ptr<HyperVMigrationTargetRecords> migration_records;
};
} // namespace multipass::hyperv
