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

#include <hyperv_api/hcs_ownership.h>

#include <multipass/path.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace multipass
{
class AvailabilityZone;
class SSHKeyProvider;
} // namespace multipass

namespace multipass::hyperv
{
struct LegacyDiskLayout;

/**
 * Human-readable phase notification emitted as the retained-copy migration progresses.
 * The daemon maps these to transient `SetReply.reply_message` updates; the engine itself
 * has no opinion on how they are surfaced.
 */
using MigrationPhaseCallback = std::function<void(std::string_view phase)>;

/**
 * Reusable retained-copy migration primitive. Migrates the legacy Hyper-V @p legacy_vm
 * into @p target_instance_dir using the supplied (already adapted) @p description, SSH
 * key, and zone. The source is never mutated: the disk graph is copied into a private,
 * target-local set, verified, trial-booted through a disposable differencing disk, and
 * only then committed. Returns the committed HCS ownership on success; throws on any
 * failure (leaving the source intact and recoverable).
 *
 * @p on_phase, when set, receives coarse progress notifications at each phase boundary.
 */
[[nodiscard]] HCSOwnership migrate_retained_copy(VirtualMachine& legacy_vm,
                                                 const VirtualMachineDescription& description,
                                                 const std::filesystem::path& target_instance_dir,
                                                 const SSHKeyProvider& key_provider,
                                                 AvailabilityZone& zone,
                                                 const std::string& primary_network_guid,
                                                 const MigrationPhaseCallback& on_phase = {});
} // namespace multipass::hyperv
