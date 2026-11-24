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

#include <scope_guard.hpp>

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
    const std::unique_lock replace_lock{replace_mutex};
    {
        const std::lock_guard lock{mutex};
        assert(entries.empty() && "load() is only for initial population");
    }
    create_missing(specs, specs);
}

void mp::VirtualMachineMounts::replace(const MountMap& specs)
{
    const std::unique_lock replace_lock{replace_mutex};

    MountMap previous;
    std::vector<std::pair<std::string, EntryPtr>> obsolete;
    {
        const std::lock_guard lock{mutex};
        for (const auto& [target, entry] : entries)
        {
            const auto dead = entry->dead.load();
            if (!dead)
                previous.emplace(target, entry->spec);

            if (const auto it = specs.find(target);
                dead || it == specs.end() || it->second != entry->spec)
            {
                entry->dead = true;
                obsolete.emplace_back(target, entry);
            }
        }
    }

    // Copies of entries may outlive removal, but their handlers must not outlive replacement.
    for (const auto& [target, entry] : obsolete)
    {
        const std::lock_guard lock{entry->mutex};
        destroy_entry_locked(target, entry);
    }

    create_missing(specs, previous);
}

void mp::VirtualMachineMounts::unload()
{
    const std::unique_lock replace_lock{replace_mutex};

    EntryMap unloaded;
    {
        const std::lock_guard lock{mutex};
        for (const auto& [_, entry] : entries)
            entry->dead = true;
        unloaded.swap(entries);
    }

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
    const std::shared_lock replace_lock{replace_mutex};
    {
        const std::lock_guard lock{mutex};
        if (entries.contains(target) || !reserved.insert(target).second)
            throw MountTargetInUseError{target, vm_name};
    }

    auto unreserve = sg::make_scope_guard([this, &target]() noexcept {
        const std::lock_guard lock{mutex};
        reserved.erase(target);
    });

    auto handler = make_handler(target, mount);
    if (is_running())
    {
        try
        {
            handler->activate(server);
        }
        catch (const SSHFSMissingError&)
        {
            throw;
        }
        catch (const std::exception& e)
        {
            throw MountActivationError{e.what()};
        }
    }

    auto entry = std::make_shared<Entry>(mount, std::move(handler));
    const std::lock_guard lock{mutex};
    reserved.erase(target);
    unreserve.dismiss();
    entries.emplace(target, std::move(entry));
    report(specs_locked());
}

void mp::VirtualMachineMounts::remove(const std::string& target)
{
    EntryPtr entry;
    {
        const std::lock_guard lock{mutex};
        const auto it = entries.find(target);
        if (it == entries.end() || it->second->dead)
            throw MountNotFoundError{target, vm_name};

        entry = it->second;
    }

    const std::lock_guard entry_lock{entry->mutex};
    if (!entry->live())
        return;

    entry->handler->deactivate();
    destroy_entry_locked(target, entry, /*report_change=*/true);
}

std::vector<std::pair<std::string, std::string>> mp::VirtualMachineMounts::remove_all()
{
    std::vector<std::pair<std::string, std::string>> failures;
    auto removed_any = false;

    for (const auto& [target, entry] : entries_copy())
    {
        const std::lock_guard lock{entry->mutex};
        try
        {
            if (!entry->live())
                continue;

            entry->handler->deactivate();
            removed_any |= destroy_entry_locked(target, entry);
        }
        catch (const std::exception& e)
        {
            failures.emplace_back(target, e.what());
        }
    }

    if (removed_any)
    {
        const std::lock_guard lock{mutex};
        report(specs_locked());
    }

    return failures;
}

mp::MountActivationResult mp::VirtualMachineMounts::activate_all(ServerVariant server)
{
    MountActivationResult result;
    auto pruned_any = false;

    for (const auto& [target, entry] : entries_copy())
    {
        const std::lock_guard lock{entry->mutex};
        if (!entry->live())
            continue;

        try
        {
            entry->handler->activate(server);
        }
        catch (const SSHFSMissingError&)
        {
            result.sshfs_missing = true;
            break;
        }
        catch (const std::exception& e)
        {
            result.pruned.emplace_back(target, e.what());
            pruned_any |= destroy_entry_locked(target, entry);
        }
    }

    if (pruned_any)
    {
        const std::lock_guard lock{mutex};
        report(specs_locked());
    }

    return result;
}

void mp::VirtualMachineMounts::stop_all()
{
    for (const auto& [_, entry] : entries_copy())
    {
        const std::lock_guard lock{entry->mutex};
        if (entry->handler)
            entry->handler->deactivate(/*force=*/true);
    }
}

auto mp::VirtualMachineMounts::specs() const -> MountMap
{
    const std::lock_guard lock{mutex};
    return specs_locked();
}

bool mp::VirtualMachineMounts::empty() const
{
    const std::lock_guard lock{mutex};
    return entries.empty() && reserved.empty();
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
    const std::lock_guard lock{mutex};
    return {entries.begin(), entries.end()};
}

bool mp::VirtualMachineMounts::destroy_entry_locked(const std::string& target,
                                                    const EntryPtr& entry,
                                                    bool report_change)
{
    entry->dead = true;
    entry->handler.reset();

    const std::lock_guard lock{mutex};
    const auto it = entries.find(target);
    if (it == entries.end() || it->second != entry)
        return false;

    entries.erase(it);
    if (report_change)
        report(specs_locked());

    return true;
}

void mp::VirtualMachineMounts::create_missing(const MountMap& specs, const MountMap& report_unless)
{
    std::vector<std::pair<std::string, VMMount>> missing;
    {
        const std::lock_guard lock{mutex};
        for (const auto& [target, spec] : specs)
        {
            if (const auto it = entries.find(target); it != entries.end())
            {
                assert(it->second->spec == spec && "obsolete mounts must be pruned first");
                continue;
            }
            missing.emplace_back(target, spec);
        }
    }

    std::vector<std::pair<std::string, EntryPtr>> created;
    created.reserve(missing.size());
    for (const auto& [target, spec] : missing)
    {
        try
        {
            created.emplace_back(target, std::make_shared<Entry>(spec, make_handler(target, spec)));
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

    const std::lock_guard lock{mutex};
    for (auto& [target, entry] : created)
        entries.emplace(std::move(target), std::move(entry));

    if (auto current = specs_locked(); current != report_unless)
        report(current);
}
