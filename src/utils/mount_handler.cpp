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

#include <multipass/mount_handler.h>

#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/logging/log.h>
#include <multipass/virtual_machine.h>

#include <cassert>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "mount-handler";
} // namespace

mp::MountHandler::MountHandler(VirtualMachine* vm,
                               const SSHKeyProvider* ssh_key_provider,
                               VMMount mount_spec,
                               const std::string& target)
    : vm{vm}, ssh_key_provider{ssh_key_provider}, mount_spec{std::move(mount_spec)}, target{target}
{
    std::error_code err;
    auto source_status = MP_FILEOPS.status(source, err);
    if (source_status.type() == fs::file_type::not_found)
        throw std::runtime_error(fmt::format("Mount source path \"{}\" does not exist.", source));
    if (err)
        throw std::runtime_error(
            fmt::format("Mount source path \"{}\" is not accessible: {}.", source, err.message()));
    if (source_status.type() != fs::file_type::directory)
        throw std::runtime_error(
            fmt::format("Mount source path \"{}\" is not a directory.", source));
    if (source_status.permissions() != fs::perms::unknown &&
        (source_status.permissions() & fs::perms::owner_read) == fs::perms::none)
        throw std::runtime_error(fmt::format("Mount source path \"{}\" is not readable.", source));
}

void mp::MountHandler::activate(ServerVariant server, std::chrono::milliseconds timeout)
{
    std::lock_guard active_lock{active_mutex};
    if (!is_active())
    {
        activate_impl(server, timeout);
        active = true; // in case record deployment throws
        update_record(&GuestMountRecord::deploy, "deploy");
    }

    assert(active);
}

void mp::MountHandler::deactivate(bool force)
{
    std::lock_guard active_lock{active_mutex};
    if (is_active())
    {
        deactivate_impl(force);
        active = false; // in case record undeployment throws
        update_record(&GuestMountRecord::undeploy, "undeploy");
    }

    active = false; // is_active may have returned false even if active was true
}

void mp::MountHandler::update_record(void (GuestMountRecord::*update)(VirtualMachine&) const,
                                     std::string_view action) const
{
    if (!vm)
        return;

    try
    {
        (record.*update)(*vm);
    }
    catch (const SSHException& e)
    {
        warn_record_failure(action, e);
    }
    catch (const IPUnavailableException& e)
    {
        warn_record_failure(action, e);
    }
}

void mp::MountHandler::warn_record_failure(std::string_view action, const std::exception& e) const
{
    mpl::warn(category,
              "Failed to {} the record of mount \"{}\" in instance '{}': {}",
              action,
              target,
              vm->get_name(),
              e.what());
}
