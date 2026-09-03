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

#include <multipass/network_interface.h>
#include <multipass/network_interface_info.h>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace multipass::hyperv
{
/**
 * Resolve each source extra interface - which references a legacy Hyper-V vSwitch by id -
 * onto the single physical adapter that switch bridges, using the backend's reported
 * @p available_networks. Order and MAC address are preserved. Throws
 * @ref InstanceMigrationError if any interface's switch is unknown, or does not bridge
 * exactly one physical adapter (i.e. it is unmappable and the instance must fail rather
 * than silently lose or remap a NIC).
 */
[[nodiscard]] std::vector<NetworkInterface> translate_extra_interfaces(
    const std::vector<NetworkInterface>& source_interfaces,
    const std::vector<NetworkInterfaceInfo>& available_networks);

// A value is the reason an instance was skipped; nullopt means it was migrated.
using InstanceMigrationResult = std::optional<std::string>;

/**
 * Thrown by an @ref InstanceMigrator for a recoverable per-instance failure. Processing
 * continues with later instances, but the final batch status becomes nonzero.
 */
class InstanceMigrationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/**
 * Thrown when the target store is left in a state that makes further commits unsafe. The
 * batch aborts immediately; earlier committed targets are retained.
 */
class MigrationAbortError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/**
 * Progress sink. Phase updates map to `SetReply.reply_message` (transient), while skips,
 * failures, and the final summary map to `SetReply.log_line` (permanent).
 */
class MigrationProgress
{
public:
    virtual ~MigrationProgress() = default;

    virtual void phase(const std::string& instance, const std::string& message) = 0;
    virtual void skipped(const std::string& instance, const std::string& reason) = 0;
    virtual void failed(const std::string& instance, const std::string& reason) = 0;
    virtual void finished(const std::vector<std::string>& migrated_names) = 0;
};

using MigrationCancellation = std::function<bool()>;

/**
 * Migrates one instance at a time. The concrete implementation owns eligibility
 * classification, the retained-copy transaction, network translation, trial boot, and the
 * target-store commit.
 */
class InstanceMigrator
{
public:
    virtual ~InstanceMigrator() = default;

    // Source names to consider. Returned order does not matter; the service sorts them
    // lexicographically before processing.
    [[nodiscard]] virtual std::vector<std::string> source_names() = 0;

    // Attempt a single instance. Emits phase updates through @p progress. Returns
    // migrated/skipped, or throws @ref InstanceMigrationError (recoverable) /
    // @ref MigrationAbortError (unsafe -> abort the batch).
    [[nodiscard]] virtual InstanceMigrationResult migrate(const std::string& name,
                                                          MigrationProgress& progress) = 0;
};

struct BulkMigrationResult
{
    bool success{true};    // false if any per-instance failure or abort occurred
    bool aborted{false};   // an unsafe target-store failure stopped the batch
    bool cancelled{false}; // RPC cancellation stopped the batch at a boundary
};

/**
 * Drive the bulk migration:
 *   - process names in stable lexicographic order;
 *   - skips alone are a success;
 *   - any per-instance failure makes the final status nonzero but processing continues;
 *   - an unsafe target-store failure aborts the remaining batch;
 *   - cancellation stops at the next per-instance boundary, retaining earlier commits.
 */
[[nodiscard]] BulkMigrationResult run_bulk_migration(InstanceMigrator& migrator,
                                                     MigrationProgress& progress,
                                                     const MigrationCancellation& cancel);
} // namespace multipass::hyperv
