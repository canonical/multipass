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

#pragma once

#include <multipass/disabled_copy_move.h>
#include <multipass/mount_handler.h>
#include <multipass/server_variant.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_mount.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace multipass
{
// Owns an instance's mount specs together with their handlers.
//
// The map lock is never held while activating or deactivating, so a slow activation does not
// block operations on other mounts. Each entry serializes handler use and destruction.
// Every change to the set of mounts is reported, under the lock, so reports arrive in order.
class VirtualMachineMounts : private DisabledCopyMove
{
public:
    using MountMap = std::unordered_map<std::string, VMMount>;
    using HandlerFactory =
        std::function<MountHandler::UPtr(const std::string& target, const VMMount& mount)>;
    using Reporter = std::function<void(const MountMap& mounts)>; // must not call back into this
    using RunningPredicate = std::function<bool()>;

    VirtualMachineMounts(std::string vm_name,
                         HandlerFactory make_handler,
                         Reporter report,
                         RunningPredicate is_running);

    // Creates handlers for the given specs, dropping (with a warning) those that cannot be
    // created. Reports only if something was dropped.
    void load(const MountMap& specs);

    // Replaces the current mounts with the given specs, keeping handlers whose spec is unchanged.
    // Nothing is activated. Reports if the resulting set differs from the previous one.
    void replace(const MountMap& specs);

    // Destroys all handlers without reporting.
    void unload();

    void add(const std::string& target, const VMMount& mount, ServerVariant server);
    void remove(const std::string& target);
    std::vector<std::pair<std::string, std::string>> remove_all();
    MountActivationResult activate_all(ServerVariant server);
    void stop_all();

    MountMap specs() const;
    bool empty() const;

private:
    struct Entry
    {
        Entry(VMMount spec, MountHandler::UPtr handler)
            : spec{std::move(spec)}, handler{std::move(handler)}
        {
        }

        const VMMount spec;
        MountHandler::UPtr handler;
        std::mutex mutex;
    };
    using EntryPtr = std::shared_ptr<Entry>;
    using EntryMap = std::unordered_map<std::string, EntryPtr>;

    MountMap specs_locked() const;
    std::vector<std::pair<std::string, EntryPtr>> entries_copy() const;
    bool erase_if_current_locked(const std::vector<std::pair<std::string, EntryPtr>>& candidates);
    void create_missing_locked(const MountMap& specs);

    const std::string vm_name;
    const HandlerFactory make_handler;
    const Reporter report;
    const RunningPredicate is_running;

    mutable std::mutex mutex;
    EntryMap entries;
    std::unordered_set<std::string> reserved; // targets with an add in flight
};
} // namespace multipass
