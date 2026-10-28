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
    const std::lock_guard lock{mutex};
    create_missing_locked(specs);

    if (auto current = specs_locked(); current != specs)
        report(current);
}

void mp::VirtualMachineMounts::replace(const MountMap& specs)
{
    MountMap previous;
    std::vector<std::pair<std::string, EntryPtr>> obsolete;
    {
        const std::lock_guard lock{mutex};
        previous = specs_locked();
        for (const auto& [target, entry] : entries)
        {
            const auto it = specs.find(target);
            if (it == specs.end() || it->second != entry->spec)
                obsolete.emplace_back(target, entry);
        }
    }

    // Copies of entries may outlive removal, but their handlers must not outlive replacement.
    for (const auto& [_, entry] : obsolete)
    {
        const std::lock_guard lock{entry->mutex};
        entry->handler.reset();
    }

    const std::lock_guard lock{mutex};
    erase_if_current_locked(obsolete);
    create_missing_locked(specs);

    if (auto current = specs_locked(); current != previous)
        report(current);
}

void mp::VirtualMachineMounts::unload()
{
    EntryMap unloaded;
    {
        const std::lock_guard lock{mutex};
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
        if (it == entries.end())
            throw MountNotFoundError{target, vm_name};

        entry = it->second;
    }

    const std::lock_guard entry_lock{entry->mutex};
    if (entry->handler)
    {
        entry->handler->deactivate();
        entry->handler.reset();
    }

    const std::lock_guard lock{mutex};
    if (erase_if_current_locked({{target, entry}}))
        report(specs_locked());
}

std::vector<std::pair<std::string, std::string>> mp::VirtualMachineMounts::remove_all()
{
    std::vector<std::pair<std::string, std::string>> failures;
    std::vector<std::pair<std::string, EntryPtr>> deactivated;

    for (const auto& [target, entry] : entries_copy())
    {
        const std::lock_guard lock{entry->mutex};
        try
        {
            if (entry->handler)
            {
                entry->handler->deactivate();
                entry->handler.reset();
            }
            deactivated.emplace_back(target, entry);
        }
        catch (const std::exception& e)
        {
            failures.emplace_back(target, e.what());
        }
    }

    const std::lock_guard lock{mutex};
    if (erase_if_current_locked(deactivated))
        report(specs_locked());

    return failures;
}

mp::MountActivationResult mp::VirtualMachineMounts::activate_all(ServerVariant server)
{
    MountActivationResult result;
    std::vector<std::pair<std::string, EntryPtr>> failed;

    for (const auto& [target, entry] : entries_copy())
    {
        const std::lock_guard lock{entry->mutex};
        if (!entry->handler)
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
            entry->handler.reset();
            failed.emplace_back(target, entry);
        }
    }

    if (!failed.empty())
    {
        const std::lock_guard lock{mutex};
        if (erase_if_current_locked(failed))
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
        ret.emplace(target, entry->spec);

    return ret;
}

auto mp::VirtualMachineMounts::entries_copy() const -> std::vector<std::pair<std::string, EntryPtr>>
{
    const std::lock_guard lock{mutex};
    return {entries.begin(), entries.end()};
}

bool mp::VirtualMachineMounts::erase_if_current_locked(
    const std::vector<std::pair<std::string, EntryPtr>>& candidates)
{
    auto erased = false;
    for (const auto& [target, entry] : candidates)
    {
        const auto it = entries.find(target);
        if (it != entries.end() && it->second == entry)
        {
            entries.erase(it);
            erased = true;
        }
    }

    return erased;
}

void mp::VirtualMachineMounts::create_missing_locked(const MountMap& specs)
{
    for (const auto& [target, spec] : specs)
    {
        if (const auto it = entries.find(target); it != entries.end())
        {
            assert(it->second->spec == spec && "obsolete mounts must be pruned first");
            continue;
        }

        try
        {
            entries.emplace(target, std::make_shared<Entry>(spec, make_handler(target, spec)));
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
}
