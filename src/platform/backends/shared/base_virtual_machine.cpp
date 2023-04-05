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
#include <multipass/json_writer.h>
#include <multipass/logging/log.h>
#include <multipass/snapshot.h>
#include <multipass/top_catch_all.h>

#include <scope_guard.hpp>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto snapshot_extension = ".snapshot.json";
}

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

std::shared_ptr<const Snapshot> BaseVirtualMachine::take_snapshot(const QDir& dir, const VMSpecs& specs,
                                                                  const std::string& name, const std::string& comment)
{
    // TODO@snapshots generate name
    {
        std::unique_lock write_lock{snapshot_mutex};

        const auto [it, success] = snapshots.try_emplace(name, nullptr);

        if (success)
        {
            auto rollback_on_failure = sg::make_scope_guard([this, it = it, old_head = head_snapshot]() noexcept {
                if (it->second) // snapshot was created
                {
                    head_snapshot = std::move(old_head);
                    mp::top_catch_all(vm_name, [it] { it->second->delet(); });
                }

                snapshots.erase(it);
            });

            // TODO@snapshots - generate implementation-specific snapshot instead
            auto ret = head_snapshot = it->second = std::make_shared<BaseSnapshot>(name, comment, head_snapshot, specs);

            persist_head_snapshot(dir);
            rollback_on_failure.dismiss();

            log_latest_snapshot(std::move(write_lock));

            return ret;
        }
    }

    mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", name));
    throw SnapshotNameTaken{vm_name, name};
}

template <typename LockT>
void BaseVirtualMachine::log_latest_snapshot(LockT lock) const
{
    auto num_snapshots = snapshots.size();
    auto parent_name = head_snapshot->get_parent_name();
    assert(bool(head_snapshot->get_parent()) == bool(num_snapshots - 1) && "null parent <!=> this is the 1st snapshot");

    if (auto log_detail_lvl = mpl::Level::debug; log_detail_lvl <= mpl::get_logging_level())
    {
        auto name = head_snapshot->get_name();
        lock.unlock(); // unlock earlier

        mpl::log(log_detail_lvl, vm_name,
                 fmt::format(R"(New snapshot: "{}"; Descendant of: "{}"; Total snapshots: {})", name, parent_name,
                             num_snapshots));
    }
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

void BaseVirtualMachine::persist_head_snapshot(const QDir& dir) const
{
    // TODO@snapshots add index to file name
    auto snapshot_record_file = dir.filePath(QString::fromStdString(head_snapshot->get_name()) + snapshot_extension);
    mp::write_json(head_snapshot->serialize(), std::move(snapshot_record_file));

    // TODO@snapshots persist snap total and head snapshot
}

} // namespace multipass
