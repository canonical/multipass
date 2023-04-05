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

auto multipass::BaseVirtualMachine::view_snapshots() const noexcept -> SnapshotVista
{
    SnapshotVista ret;

    std::shared_lock read_lock{snapshot_mutex};
    ret.reserve(snapshots.size());
    std::transform(std::cbegin(snapshots), std::cend(snapshots), std::back_inserter(ret),
                   [](const auto& pair) { return pair.second; });

    return ret;
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::get_snapshot(const std::string& name) const
{
    return snapshots.at(name);
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::take_snapshot(const VMSpecs& specs, const std::string& name,
                                                                  const std::string& comment)
{
    // TODO@snapshots generate name
    // TODO@snapshots generate implementation-specific snapshot instead

    {
        std::unique_lock write_lock{snapshot_mutex};

        const auto [it, success] =
            snapshots.try_emplace(name, std::make_shared<BaseSnapshot>(name, comment, head_snapshot, specs));

        if (success)
        {
            auto ret = head_snapshot = it->second;
            auto num_snapshots = snapshots.size();
            auto parent_name = ret->get_parent_name();
            assert(bool(ret->get_parent()) == bool(num_snapshots - 1) && "null parent <!=> this is the 1st snapshot");

            write_lock.unlock();

            if (auto log_detail_lvl = mpl::Level::debug; log_detail_lvl <= mpl::get_logging_level())
                mpl::log(log_detail_lvl, vm_name,
                         fmt::format("New snapshot: {}; Descendant of: {}; Total snapshots: {}", name, parent_name,
                                     num_snapshots));

            return ret;
        }
    }

    mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", name));
    throw SnapshotNameTaken{vm_name, name};
}

void BaseVirtualMachine::load_snapshot(const QJsonObject& json)
{
    // TODO@snapshots move to specific VM implementations and make specific snapshot from there
    auto snapshot = std::make_shared<BaseSnapshot>(json, *this);
    const auto& name = snapshot->get_name();
    auto [it, success] = snapshots.try_emplace(name, snapshot);

    if (!success)
    {
        mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", name));
        throw SnapshotNameTaken{vm_name, name};
    }

    head_snapshot = it->second; // TODO@snapshots persist/load this separately
}

} // namespace multipass
