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
#include "daemon/vm_specs.h" // TODO@snapshots move this

#include <multipass/exceptions/file_not_found_exception.h>
#include <multipass/exceptions/snapshot_name_taken.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/file_ops.h>
#include <multipass/json_writer.h>
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
constexpr auto yes_overwrite = true;
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

        auto rollback_on_failure = sg::make_scope_guard( // best effort to rollback
            [this, it = it, old_head = head_snapshot, old_count = snapshot_count]() mutable noexcept {
                if (old_head != head_snapshot)
                {
                    assert(it->second && "snapshot not created despite modified head");
                    if (snapshot_count > old_count) // snapshot was created
                    {
                        assert(snapshot_count - old_count == 1);
                        --snapshot_count;

                        mp::top_catch_all(vm_name, [it] { it->second->erase(); });
                    }

                    head_snapshot = std::move(old_head);
                }

                snapshots.erase(it);
            });

        auto ret = head_snapshot = it->second = make_specific_snapshot(snapshot_name, comment, head_snapshot, specs);
        ret->capture();

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
    auto snapshot = make_specific_snapshot(json);
    const auto& name = snapshot->get_name();
    auto [it, success] = snapshots.try_emplace(name, snapshot);

    if (!success)
    {
        mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", name));
        throw SnapshotNameTaken{vm_name, name};
    }
}

auto BaseVirtualMachine::make_head_file_rollback(const QString& head_path, QFile& head_file) const
{
    return sg::make_scope_guard([this, &head_path, &head_file, old_head = head_snapshot->get_parent_name(),
                                 existed = head_file.exists()]() noexcept {
        head_file_rollback_guts(head_path, head_file, old_head, existed);
    });
}

void BaseVirtualMachine::head_file_rollback_guts(const QString& head_path, QFile& head_file,
                                                 const std::string& old_head, bool existed) const
{
    // best effort, ignore returns
    if (!existed)
        head_file.remove();
    else
        top_catch_all(vm_name, [&head_path, &old_head] {
            MP_UTILS.make_file_with_content(head_path.toStdString(), old_head, yes_overwrite);
        });
}

void BaseVirtualMachine::persist_head_snapshot(const QDir& snapshot_dir) const
{
    assert(head_snapshot);

    const auto snapshot_filename = QString{"%1-%2.%3"}
                                       .arg(snapshot_count, index_digits, 10, QLatin1Char('0'))
                                       .arg(QString::fromStdString(head_snapshot->get_name()), snapshot_extension);

    auto snapshot_path = snapshot_dir.filePath(snapshot_filename);
    auto head_path = derive_head_path(snapshot_dir);
    auto count_path = snapshot_dir.filePath(count_filename);

    auto rollback_snapshot_file = sg::make_scope_guard([&snapshot_filename]() noexcept {
        QFile{snapshot_filename}.remove(); // best effort, ignore return
    });

    mp::write_json(head_snapshot->serialize(), snapshot_path);

    QFile head_file{head_path};

    auto head_file_rollback = make_head_file_rollback(head_path, head_file);

    persist_head_snapshot_name(head_path);
    MP_UTILS.make_file_with_content(count_path.toStdString(), std::to_string(snapshot_count), yes_overwrite);

    rollback_snapshot_file.dismiss();
    head_file_rollback.dismiss();
}

QString BaseVirtualMachine::derive_head_path(const QDir& snapshot_dir) const
{
    return snapshot_dir.filePath(head_filename);
}

void BaseVirtualMachine::persist_head_snapshot_name(const QString& head_path) const
{
    MP_UTILS.make_file_with_content(head_path.toStdString(), head_snapshot->get_name(), yes_overwrite);
}

std::string BaseVirtualMachine::generate_snapshot_name() const
{
    return fmt::format("snapshot{}", snapshot_count + 1);
}

auto BaseVirtualMachine::make_restore_rollback(const QString& head_path, VMSpecs& specs)
{
    return sg::make_scope_guard([this, &head_path, old_head = head_snapshot, old_specs = specs, &specs]() noexcept {
        top_catch_all(vm_name, &BaseVirtualMachine::restore_rollback_guts, this, head_path, old_head, old_specs, specs);
    });
}

void BaseVirtualMachine::restore_rollback_guts(const QString& head_path, const std::shared_ptr<Snapshot>& old_head,
                                               const VMSpecs& old_specs, VMSpecs& specs)
{
    // best effort only
    old_head->apply();
    specs = old_specs;
    if (old_head != head_snapshot)
    {
        head_snapshot = old_head;
        persist_head_snapshot_name(head_path);
    }
}

void BaseVirtualMachine::restore_snapshot(const QDir& snapshot_dir, const std::string& name, VMSpecs& specs)
{
    using St [[maybe_unused]] = VirtualMachine::State;

    std::unique_lock lock{snapshot_mutex};
    assert(state == St::off || state == St::stopped);

    auto snapshot = snapshots.at(name); // TODO@snapshots convert out_of_range exception, here and `get_snapshot`

    // TODO@snapshots convert into runtime_errors (persisted info could have been tampered with)
    assert(specs.disk_space == snapshot->get_disk_space() && "resizing VMs with snapshots isn't yet supported");
    assert(snapshot->get_state() == St::off || snapshot->get_state() == St::stopped);

    snapshot->apply();

    const auto head_path = derive_head_path(snapshot_dir);
    auto rollback = make_restore_rollback(head_path, specs);

    specs.state = snapshot->get_state();
    specs.num_cores = snapshot->get_num_cores();
    specs.mem_size = snapshot->get_mem_size();
    specs.mounts = snapshot->get_mounts();
    specs.metadata = snapshot->get_metadata();

    if (head_snapshot != snapshot)
    {
        head_snapshot = snapshot;
        persist_head_snapshot_name(head_path);
    }

    rollback.dismiss();
}

} // namespace multipass
