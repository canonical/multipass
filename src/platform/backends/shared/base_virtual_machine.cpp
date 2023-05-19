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

#include <multipass/exceptions/file_not_found_exception.h>
#include <multipass/exceptions/snapshot_name_taken.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/snapshot.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>

#include <scope_guard.hpp>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto snapshot_extension = "snapshot.json";
constexpr auto head_filename = "snapshot-head";
constexpr auto count_filename = "snapshot-count";
constexpr auto index_digits = 4;        // these two go together
constexpr auto max_snapshots = 1000ull; // replace suffix with uz for size_t in C++23
} // namespace

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

    const std::unique_lock lock{snapshot_mutex};
    ret.reserve(snapshots.size());
    std::transform(std::cbegin(snapshots), std::cend(snapshots), std::back_inserter(ret),
                   [](const auto& pair) { return pair.second; });

    return ret;
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::get_snapshot(const std::string& name) const
{
    const std::unique_lock lock{snapshot_mutex};
    return snapshots.at(name);
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::take_snapshot(const QDir& snapshot_dir, const VMSpecs& specs,
                                                                  const std::string& name, const std::string& comment)
{
    std::string snapshot_name;

    {
        std::unique_lock lock{snapshot_mutex};
        if (snapshot_count > max_snapshots)
            throw std::runtime_error{fmt::format("Maximum number of snapshots exceeded", max_snapshots)};
        snapshot_name = name.empty() ? generate_snapshot_name() : name;

        const auto [it, success] = snapshots.try_emplace(snapshot_name, nullptr);
        if (!success)
        {
            mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", snapshot_name));
            throw SnapshotNameTaken{vm_name, snapshot_name};
        }

        auto rollback_on_failure = sg::make_scope_guard([this, it = it, old_head = head_snapshot]() mutable noexcept {
            if (it->second) // snapshot was created
            {
                --snapshot_count;
                head_snapshot = std::move(old_head);
                mp::top_catch_all(vm_name, [it] { it->second->delet(); });
            }

            snapshots.erase(it);
        });

        auto ret = head_snapshot = it->second = make_specific_snapshot(snapshot_name, comment, head_snapshot, specs);
        ret->shoot();

        ++snapshot_count;
        persist_head_snapshot(snapshot_dir);
        rollback_on_failure.dismiss();

        log_latest_snapshot(std::move(lock));

        return ret;
    }
}

void BaseVirtualMachine::load_snapshots(const QDir& snapshot_dir)
{
    std::unique_lock lock{snapshot_mutex};

    auto snapshot_files = MP_FILEOPS.entryInfoList(snapshot_dir, {QString{"*.%1"}.arg(snapshot_extension)},
                                                   QDir::Filter::Files | QDir::Filter::Readable, QDir::SortFlag::Name);
    for (const auto& finfo : snapshot_files)
        load_snapshot_from_file(finfo.filePath());

    load_generic_snapshot_info(snapshot_dir);
}

void BaseVirtualMachine::load_generic_snapshot_info(const QDir& snapshot_dir)
{
    try
    {
        snapshot_count = std::stoull(mpu::contents_of(snapshot_dir.filePath(count_filename)));
        head_snapshot = snapshots.at(mpu::contents_of(snapshot_dir.filePath(head_filename)));
    }
    catch (FileOpenFailedException&)
    {
        if (!snapshots.empty())
            throw;
    }
}

template <typename LockT>
void BaseVirtualMachine::log_latest_snapshot(LockT lock) const
{
    auto num_snapshots = snapshots.size();
    auto parent_name = head_snapshot->get_parent_name();

    assert(num_snapshots <= snapshot_count && "can't have more snapshots than were ever taken");
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

void BaseVirtualMachine::load_snapshot_from_file(const QString& filename)
{
    QFile file{filename};
    if (!MP_FILEOPS.open(file, QIODevice::ReadOnly))
        throw std::runtime_error{fmt::v9::format("Could not open snapshot file for for reading: {}", file.fileName())};

    QJsonParseError parse_error{};
    const auto& data = MP_FILEOPS.read_all(file);

    if (auto json = QJsonDocument::fromJson(data, &parse_error).object(); parse_error.error)
        throw std::runtime_error{fmt::v9::format("Could not parse snapshot JSON; error: {}; file: {}", file.fileName(),
                                                 parse_error.errorString())};
    else if (json.isEmpty())
        throw std::runtime_error{fmt::v9::format("Empty snapshot JSON: {}", file.fileName())};
    else
        load_snapshot(json);
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
}

void BaseVirtualMachine::persist_head_snapshot(const QDir& snapshot_dir) const
{
    assert(head_snapshot);

    const auto snapshot_filename = QString{"%1-%2.%3"}
                                       .arg(snapshot_count, index_digits, 10, QLatin1Char('0'))
                                       .arg(QString::fromStdString(head_snapshot->get_name()), snapshot_extension);

    auto snapshot_path = snapshot_dir.filePath(snapshot_filename);
    auto head_path = snapshot_dir.filePath(head_filename);
    auto count_path = snapshot_dir.filePath(count_filename);

    auto rollback_snapshot_file = sg::make_scope_guard([&snapshot_filename]() noexcept {
        QFile{snapshot_filename}.remove(); // best effort, ignore return
    });

    mp::write_json(head_snapshot->serialize(), snapshot_path);

    auto overwrite = true;
    QFile head_file{head_path};

    auto rollback_head_file =
        sg::make_scope_guard([this, &head_path, &head_file, overwrite, old_head = head_snapshot->get_parent_name(),
                              existed = head_file.exists()]() noexcept {
            // best effort, ignore returns
            if (!existed)
                head_file.remove();
            else
                mp::top_catch_all(vm_name, [&head_path, &old_head, overwrite] {
                    MP_UTILS.make_file_with_content(head_path.toStdString(), old_head, overwrite);
                });
        });

    MP_UTILS.make_file_with_content(head_path.toStdString(), head_snapshot->get_name(), overwrite);
    MP_UTILS.make_file_with_content(count_path.toStdString(), std::to_string(snapshot_count), overwrite);

    rollback_snapshot_file.dismiss();
    rollback_head_file.dismiss();
}

std::string BaseVirtualMachine::generate_snapshot_name() const
{
    return fmt::format("snapshot{}", snapshot_count + 1);
}

} // namespace multipass
