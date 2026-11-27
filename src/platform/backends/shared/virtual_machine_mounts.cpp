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

#include "virtual_machine_mounts.h"

#include <multipass/exceptions/mount_exceptions.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/logging/log.h>

#include <cassert>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::VirtualMachineMounts::VirtualMachineMounts(std::string vm_name,
                                               HandlerFactory make_handler,
                                               Reporter report,
                                               RunningPredicate is_running)
    : vm_name{std::move(vm_name)},
      make_handler{std::move(make_handler)},
      report{std::move(report)},
      is_running{std::move(is_running)}
{
}

void mp::VirtualMachineMounts::load(const MountMap& specs)
{
    const std::lock_guard update_lock{update_mutex};
    assert(empty() && "load() is only for initial population");
    if (!specs.empty())
        mpl::debug(vm_name, "Loading {} mount(s)", specs.size());
    auto created = make_entries(specs);
    const std::lock_guard lock{map_mutex};
    entries.merge(created);
    if (entries.size() != specs.size())
        report(specs_locked());
}

void mp::VirtualMachineMounts::replace(const MountMap& specs)
{
    const std::lock_guard update_lock{update_mutex};

    const auto removed = retire_obsolete_entries(specs);
    auto created = make_entries(missing_specs(specs));
    if (removed > 0 || !created.empty())
    {
        const auto added = created.size();
        {
            const std::lock_guard lock{map_mutex};
            entries.merge(created);
            report(specs_locked());
        }
        mpl::info(vm_name, "Replaced mounts (removed: {}, added: {})", removed, added);
    }
}

std::size_t mp::VirtualMachineMounts::retire_obsolete_entries(const MountMap& specs)
{
    struct ObsoleteEntry
    {
        std::string target;
        EntryPtr entry;
        const char* reason;
    };

    std::size_t removed = 0;
    std::vector<ObsoleteEntry> obsolete;
    {
        const std::lock_guard lock{map_mutex};
        for (const auto& [target, entry] : entries)
        {
            const auto dead = entry->dead.load();
            const auto it = specs.find(target);
            const char* reason = dead                      ? "already removed"
                               : it == specs.end()         ? "removed"
                               : it->second != entry->spec ? "spec changed"
                                                           : nullptr;
            if (reason)
            {
                entry->dead = true;
                if (!dead)
                    ++removed;
                obsolete.push_back({target, entry, reason});
            }
        }
    }

    // Copies of entries may outlive removal, but their handlers must not outlive replacement.
    for (const auto& [target, entry, reason] : obsolete)
    {
        mpl::debug(vm_name, R"(Retiring mount "{}" ({}))", target, reason);
        const std::lock_guard lock{entry->mutex};
        destroy_entry_locked(target, entry);
    }

    return removed;
}

auto mp::VirtualMachineMounts::missing_specs(MountMap requested) const -> MountMap
{
    const std::lock_guard lock{map_mutex};
    for (const auto& [target, _] : entries)
        requested.erase(target);
    return requested;
}

void mp::VirtualMachineMounts::unload()
{
    const std::lock_guard update_lock{update_mutex};

    EntryMap unloaded;
    {
        const std::lock_guard lock{map_mutex};
        for (const auto& [_, entry] : entries)
            entry->dead = true;
        unloaded.swap(entries);
    }

    if (!unloaded.empty())
        mpl::debug(vm_name, "Unloading {} mount(s)", unloaded.size());

    for (const auto& [_, entry] : unloaded)
    {
        const std::lock_guard lock{entry->mutex};
        entry->handler.reset();
    }
}

void mp::VirtualMachineMounts::add(const std::string& target,
                                   const VMMount& mount,
                                   ServerVariant server)
{
    const std::lock_guard update_lock{update_mutex};
    ensure_target_available(target);

    auto handler = make_handler(target, mount);
    if (is_running())
    {
        mpl::debug(vm_name, R"(Activating mount "{}" => "{}")", mount.get_source_path(), target);
        try
        {
            handler->activate(server);
        }
        catch (const SSHFSMissingError&)
        {
            mpl::debug(vm_name, R"(Rolling back mount "{}": sshfs is missing)", target);
            throw;
        }
        catch (const std::exception& e)
        {
            mpl::debug(vm_name,
                       R"(Rolling back mount "{}" after activation failure: {})",
                       target,
                       e.what());
            throw MountActivationError{e.what()};
        }
    }
    else
    {
        mpl::debug(vm_name,
                   R"(Adding mount "{}" => "{}" (deferred until instance starts))",
                   mount.get_source_path(),
                   target);
    }

    publish_entry(target, std::make_shared<Entry>(mount, std::move(handler)));
}

void mp::VirtualMachineMounts::ensure_target_available(const std::string& target) const
{
    const std::lock_guard lock{map_mutex};
    if (entries.contains(target))
        throw MountTargetInUseError{target, vm_name};
}

void mp::VirtualMachineMounts::publish_entry(const std::string& target, EntryPtr entry)
{
    const std::lock_guard lock{map_mutex};
    entries.emplace(target, std::move(entry));
    report(specs_locked());
}

void mp::VirtualMachineMounts::remove(const std::string& target)
{
    EntryPtr entry;
    {
        const std::lock_guard lock{map_mutex};
        const auto it = entries.find(target);
        if (it == entries.end() || it->second->dead)
            throw MountNotFoundError{target, vm_name};

        entry = it->second;
    }

    const std::lock_guard entry_lock{entry->mutex};
    if (!entry->live())
    {
        mpl::debug(vm_name, R"(Skipping removal of mount "{}": already removed)", target);
        return;
    }

    entry->handler->deactivate();
    destroy_entry_locked(target, entry, /*report_change=*/true);
}

std::vector<std::pair<std::string, std::string>> mp::VirtualMachineMounts::remove_all()
{
    std::vector<std::pair<std::string, std::string>> failures;
    auto removed_any = false;
    const auto current = entries_copy();

    if (!current.empty())
        mpl::debug(vm_name, "Removing {} mount(s)", current.size());

    for (const auto& [target, entry] : current)
    {
        const std::lock_guard lock{entry->mutex};
        try
        {
            if (!entry->live())
            {
                mpl::debug(vm_name, R"(Skipping removal of mount "{}": already removed)", target);
                continue;
            }

            entry->handler->deactivate();
            removed_any |= destroy_entry_locked(target, entry);
        }
        catch (const std::exception& e)
        {
            mpl::warn(vm_name,
                      R"(Failed to remove mount "{}" from '{}': {})",
                      target,
                      vm_name,
                      e.what());
            failures.emplace_back(target, e.what());
        }
    }

    if (removed_any)
    {
        const std::lock_guard lock{map_mutex};
        report(specs_locked());
    }

    return failures;
}

mp::MountActivationResult mp::VirtualMachineMounts::activate_all(ServerVariant server)
{
    MountActivationResult result;
    auto pruned_any = false;
    const auto current = entries_copy();

    if (!current.empty())
        mpl::debug(vm_name, "Activating {} mount(s)", current.size());

    for (const auto& [target, entry] : current)
    {
        const std::lock_guard lock{entry->mutex};
        if (!entry->live())
        {
            mpl::debug(vm_name, R"(Skipping activation of mount "{}": already removed)", target);
            continue;
        }

        try
        {
            entry->handler->activate(server);
        }
        catch (const SSHFSMissingError&)
        {
            mpl::warn(vm_name,
                      R"(Stopping mount activation at "{}": sshfs is missing in '{}')",
                      target,
                      vm_name);
            result.sshfs_missing = true;
            break;
        }
        catch (const std::exception& e)
        {
            mpl::warn(vm_name, R"(Removing mount "{}" from '{}': {})", target, vm_name, e.what());
            result.pruned.emplace_back(target, e.what());
            pruned_any |= destroy_entry_locked(target, entry);
        }
    }

    if (pruned_any)
    {
        const std::lock_guard lock{map_mutex};
        report(specs_locked());
    }

    return result;
}

void mp::VirtualMachineMounts::stop_all()
{
    const auto current = entries_copy();
    if (!current.empty())
        mpl::debug(vm_name, "Stopping {} mount(s)", current.size());

    for (const auto& [_, entry] : current)
    {
        const std::lock_guard lock{entry->mutex};
        if (entry->handler)
            entry->handler->deactivate(/*force=*/true);
    }
}

auto mp::VirtualMachineMounts::specs() const -> MountMap
{
    const std::lock_guard lock{map_mutex};
    return specs_locked();
}

bool mp::VirtualMachineMounts::empty() const
{
    const std::lock_guard lock{map_mutex};
    return entries.empty();
}

auto mp::VirtualMachineMounts::specs_locked() const -> MountMap
{
    MountMap ret;
    for (const auto& [target, entry] : entries)
        if (!entry->dead)
            ret.emplace(target, entry->spec);

    return ret;
}

auto mp::VirtualMachineMounts::entries_copy() const -> std::vector<std::pair<std::string, EntryPtr>>
{
    const std::lock_guard lock{map_mutex};
    return {entries.begin(), entries.end()};
}

bool mp::VirtualMachineMounts::destroy_entry_locked(const std::string& target,
                                                    const EntryPtr& entry,
                                                    bool report_change)
{
    entry->dead = true;
    entry->handler.reset();

    const std::lock_guard lock{map_mutex};
    const auto it = entries.find(target);
    if (it == entries.end() || it->second != entry)
        return false;

    entries.erase(it);
    if (report_change)
        report(specs_locked());

    return true;
}

auto mp::VirtualMachineMounts::make_entries(const MountMap& specs) const -> EntryMap
{
    EntryMap created;
    for (const auto& [target, spec] : specs)
    {
        try
        {
            created.emplace(target, std::make_shared<Entry>(spec, make_handler(target, spec)));
        }
        catch (const std::exception& e)
        {
            mpl::warn(vm_name,
                      R"(Removing mount "{}" => "{}" from '{}': {})",
                      spec.get_source_path(),
                      target,
                      vm_name,
                      e.what());
        }
    }

    return created;
}
