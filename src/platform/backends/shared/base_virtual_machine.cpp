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

#include "base_virtual_machine.h"
#include "base_snapshot.h" // TODO@snapshots may be able to remove this

#include <multipass/exceptions/snapshot_name_taken.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/logging/log.h>
#include <multipass/snapshot.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace multipass
{

BaseVirtualMachine::BaseVirtualMachine(VirtualMachine::State state, const std::string& vm_name)
    : VirtualMachine(state, vm_name){};

BaseVirtualMachine::BaseVirtualMachine(const std::string& vm_name) : VirtualMachine(vm_name){};

BaseVirtualMachine::~BaseVirtualMachine() = default; // Snapshot is now fully defined, this can call unique_ptr's dtor

std::vector<std::string> BaseVirtualMachine::get_all_ipv4(const SSHKeyProvider& key_provider)
{
    std::vector<std::string> all_ipv4;

    if (current_state() == State::running)
    {
        QString ip_a_output;

        try
        {
            SSHSession session{ssh_hostname(), ssh_port(), ssh_username(), key_provider};

            ip_a_output = QString::fromStdString(
                mpu::run_in_ssh_session(session, "ip -brief -family inet address show scope global"));

            QRegularExpression ipv4_re{QStringLiteral("([\\d\\.]+)\\/\\d+\\s*(metric \\d+)?\\s*$"),
                                       QRegularExpression::MultilineOption};

            QRegularExpressionMatchIterator ip_it = ipv4_re.globalMatch(ip_a_output);

            while (ip_it.hasNext())
            {
                auto ip_match = ip_it.next();
                auto ip = ip_match.captured(1).toStdString();

                all_ipv4.push_back(ip);
            }
        }
        catch (const SSHException& e)
        {
            mpl::log(mpl::Level::debug, vm_name, fmt::format("Error getting extra IP addresses: {}", e.what()));
        }
    }

    return all_ipv4;
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::take_snapshot(const VMSpecs& specs, const std::string& name,
                                                                  const std::string& comment)
{
    // TODO@snapshots generate name
    // TODO@snapshots generate implementation-specific snapshot instead

    {
        std::unique_lock write_lock{snapshot_mutex};

        const auto [it, success] =
            snapshots.try_emplace(name, std::make_unique<BaseSnapshot>(name, comment, head_snapshot, specs));

        if (success)
        {
            head_snapshot = it->second.get();

            // No writing from this point on
            std::shared_lock read_lock{snapshot_mutex, std::defer_lock};
            { // TODO@snapshots might as well make this into a generic util
                std::unique_lock transfer_lock{transfer_mutex}; // lock transfer from write to read lock

                write_lock.unlock();
                read_lock.lock();
            }

            if (auto log_detail_lvl = mpl::Level::debug; log_detail_lvl <= mpl::get_logging_level())
            {
                auto num_snapshots = snapshots.size();
                auto* parent = head_snapshot->get_parent();

                assert(bool(parent) == bool(num_snapshots - 1) && "null parent <!=> this is the 1st snapshot");
                const auto& parent_name = parent ? parent->get_name() : "--";

                mpl::log(log_detail_lvl, vm_name,
                         fmt::format("New snapshot: {}; Descendant of: {}; Total snapshots: {}", name, parent_name,
                                     num_snapshots));
            }

            return {*head_snapshot, std::move(read_lock)};
        }
    }

    mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", name));
    throw SnapshotNameTaken{vm_name, name};
}

} // namespace multipass
