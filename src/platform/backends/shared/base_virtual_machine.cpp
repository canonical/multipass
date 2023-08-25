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
#include <multipass/exceptions/snapshot_exceptions.h>
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
#include <QTemporaryDir>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
using St = mp::VirtualMachine::State;
constexpr auto snapshot_extension = "snapshot.json";
constexpr auto head_filename = "snapshot-head";
constexpr auto count_filename = "snapshot-count";
constexpr auto index_digits = 4; // these two go together
constexpr auto max_snapshots = 1000;
constexpr auto yes_overwrite = true;

void assert_vm_stopped(St state)
{
    assert(state == St::off || state == St::stopped);
}

mp::Path derive_head_path(const QDir& snapshot_dir)
{
    return snapshot_dir.filePath(head_filename);
}

QString derive_index_string(int index)
{
    return QString{"%1"}.arg(index, index_digits, 10, QLatin1Char('0'));
}

QString derive_snapshot_filename(const QString& index, const QString& name)
{
    return QString{"%1-%2.%3"}.arg(index, name, snapshot_extension);
}

QFileInfo find_snapshot_file(const QDir& snapshot_dir, const std::string& snapshot_name)
{
    auto index_wildcard = "????";
    auto pattern = derive_snapshot_filename(index_wildcard, QString::fromStdString(snapshot_name));
    auto files = MP_FILEOPS.entryInfoList(snapshot_dir, {pattern}, QDir::Filter::Files | QDir::Filter::Readable);

    if (auto num_found = files.count(); !num_found)
        throw std::runtime_error{fmt::format("Could not find snapshot file for pattern '{}'", pattern)};
    else if (num_found > 1)
        throw std::runtime_error{fmt::format("Multiple snapshot files found for pattern '{}'", pattern)};

    return files.first();
}

void update_parents_rollback_helper(const std::shared_ptr<mp::Snapshot>& deleted_parent,
                                    std::unordered_map<mp::Snapshot*, mp::Path>& updated_snapshot_paths)
{
    for (auto [snapshot, snapshot_filepath] : updated_snapshot_paths)
    {
        snapshot->set_parent(deleted_parent);
        if (!snapshot_filepath.isEmpty())
            mp::write_json(snapshot->serialize(), snapshot_filepath);
    }
}
} // namespace

namespace multipass
{

BaseVirtualMachine::BaseVirtualMachine(St state, const std::string& vm_name) : VirtualMachine(state, vm_name){};

BaseVirtualMachine::BaseVirtualMachine(const std::string& vm_name) : VirtualMachine(vm_name){};

std::vector<std::string> BaseVirtualMachine::get_all_ipv4(const SSHKeyProvider& key_provider)
{
    std::vector<std::string> all_ipv4;

    if (current_state() == St::running)
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

auto BaseVirtualMachine::view_snapshots() const noexcept -> SnapshotVista
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
    try
    {
        return snapshots.at(name);
    }
    catch (const std::out_of_range&)
    {
        throw NoSuchSnapshot{vm_name, name};
    }
}

std::shared_ptr<Snapshot> BaseVirtualMachine::get_snapshot(const std::string& name)
{
    return std::const_pointer_cast<Snapshot>(std::as_const(*this).get_snapshot(name));
}

void BaseVirtualMachine::take_snapshot_rollback_helper(SnapshotMap::iterator it, std::shared_ptr<Snapshot>& old_head,
                                                       int old_count)
{
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
}

auto BaseVirtualMachine::make_take_snapshot_rollback(SnapshotMap::iterator it)
{
    return sg::make_scope_guard( // best effort to rollback
        [this, it = it, old_head = head_snapshot, old_count = snapshot_count]() mutable noexcept {
            take_snapshot_rollback_helper(it, old_head, old_count);
        });
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::take_snapshot(const QDir& snapshot_dir, const VMSpecs& specs,
                                                                  const std::string& name, const std::string& comment)
{
    std::string snapshot_name;

    {
        std::unique_lock lock{snapshot_mutex};
        assert_vm_stopped(state); // precondition

        if (snapshot_count > max_snapshots)
            throw std::runtime_error{fmt::format("Maximum number of snapshots exceeded", max_snapshots)};
        snapshot_name = name.empty() ? generate_snapshot_name() : name;

        const auto [it, success] = snapshots.try_emplace(snapshot_name, nullptr);
        if (!success)
        {
            mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", snapshot_name));
            throw SnapshotNameTaken{vm_name, snapshot_name};
        }

        auto rollback_on_failure = make_take_snapshot_rollback(it);

        auto ret = head_snapshot = it->second = make_specific_snapshot(snapshot_name, comment, specs, head_snapshot);
        ret->capture();

        ++snapshot_count;
        persist_head_snapshot(snapshot_dir);

        rollback_on_failure.dismiss();
        log_latest_snapshot(std::move(lock));

        return ret;
    }
}

bool BaseVirtualMachine::updated_deleted_head(std::shared_ptr<Snapshot>& snapshot, const Path& head_path)
{
    if (head_snapshot == snapshot)
    {
        head_snapshot = snapshot->get_parent();
        persist_head_snapshot_name(head_path);
        return true;
    }

    return false;
}

auto BaseVirtualMachine::make_deleted_head_rollback(const Path& head_path, const bool& wrote_head)
{
    return sg::make_scope_guard([this, old_head = head_snapshot, &head_path, &wrote_head]() mutable noexcept {
        deleted_head_rollback_helper(head_path, wrote_head, old_head);
    });
}

void BaseVirtualMachine::deleted_head_rollback_helper(const Path& head_path, const bool& wrote_head,
                                                      std::shared_ptr<Snapshot>& old_head)
{
    if (head_snapshot != old_head)
    {
        head_snapshot = std::move(old_head);
        if (wrote_head)
            top_catch_all(vm_name, [this, &head_path] {
                MP_UTILS.make_file_with_content(head_path.toStdString(), head_snapshot->get_name(), yes_overwrite);
            });
    }
}

auto BaseVirtualMachine::make_parent_update_rollback(const std::shared_ptr<Snapshot>& deleted_parent,
                                                     std::unordered_map<Snapshot*, Path>& updated_snapshot_paths) const
{
    return sg::make_scope_guard([this, &updated_snapshot_paths, deleted_parent]() noexcept {
        top_catch_all(vm_name, &update_parents_rollback_helper, deleted_parent, updated_snapshot_paths);
    });
}

void BaseVirtualMachine::delete_snapshot_helper(const QDir& snapshot_dir, std::shared_ptr<Snapshot>& snapshot)
{
    // Remove snapshot file
    QTemporaryDir tmp_dir{};
    if (!tmp_dir.isValid())
        throw std::runtime_error{"Could not create temporary directory"};

    auto snapshot_fileinfo = find_snapshot_file(snapshot_dir, snapshot->get_name());
    auto snapshot_filepath = snapshot_fileinfo.filePath();
    auto deleting_filepath = tmp_dir.filePath(snapshot_fileinfo.fileName());

    if (!QFile{snapshot_filepath}.rename(deleting_filepath))
        throw std::runtime_error{
            fmt::format("Failed to move snapshot file to temporary destination: {}", deleting_filepath)};

    auto rollback_snapshot_file = sg::make_scope_guard([&deleting_filepath, &snapshot_filepath]() noexcept {
        QFile{deleting_filepath}.rename(snapshot_filepath); // best effort, ignore return
    });

    // Update head if deleted
    auto wrote_head = false;
    auto head_path = derive_head_path(snapshot_dir);
    auto rollback_head = make_deleted_head_rollback(head_path, wrote_head);
    wrote_head = updated_deleted_head(snapshot, head_path);

    // Update children of deleted snapshot
    std::unordered_map<Snapshot*, QString> updated_snapshot_paths;
    updated_snapshot_paths.reserve(snapshots.size());

    auto rollback_parent_updates = make_parent_update_rollback(snapshot, updated_snapshot_paths);
    update_parents(snapshot_dir, snapshot, updated_snapshot_paths);

    // Erase the snapshot with the backend and dismiss rollbacks on success
    snapshot->erase();
    rollback_parent_updates.dismiss();
    rollback_head.dismiss();
    rollback_snapshot_file.dismiss();
}

void BaseVirtualMachine::update_parents(const QDir& snapshot_dir, std::shared_ptr<Snapshot>& deleted_parent,
                                        std::unordered_map<Snapshot*, Path>& updated_snapshot_paths)
{
    auto new_parent = deleted_parent->get_parent();
    for (auto& [ignore, other] : snapshots)
    {
        if (other->get_parent() == deleted_parent)
        {
            other->set_parent(new_parent);
            updated_snapshot_paths[other.get()];

            const auto other_filepath = find_snapshot_file(snapshot_dir, other->get_name()).filePath();
            write_json(other->serialize(), other_filepath);
            updated_snapshot_paths[other.get()] = other_filepath;
        }
    }
}

void BaseVirtualMachine::delete_snapshot(const QDir& snapshot_dir, const std::string& name)
{
    std::unique_lock lock{snapshot_mutex};

    auto it = snapshots.find(name);
    if (it == snapshots.end())
        throw NoSuchSnapshot{vm_name, name};

    auto snapshot = it->second;
    delete_snapshot_helper(snapshot_dir, snapshot);

    snapshots.erase(it); // doesn't throw
    mpl::log(mpl::Level::debug, vm_name, fmt::format("Snapshot deleted: {}", name));
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

std::vector<std::string> BaseVirtualMachine::get_childrens_names(const Snapshot* parent) const
{
    std::vector<std::string> children;

    for (const auto& snapshot : view_snapshots())
        if (snapshot->get_parent().get() == parent)
            children.push_back(snapshot->get_name());

    return children;
}

void BaseVirtualMachine::load_generic_snapshot_info(const QDir& snapshot_dir)
{
    try
    {
        snapshot_count = std::stoi(mpu::contents_of(snapshot_dir.filePath(count_filename)));

        auto head_name = mpu::contents_of(snapshot_dir.filePath(head_filename));
        head_snapshot = head_name.empty() ? nullptr : get_snapshot(head_name);
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
    auto num_snapshots = static_cast<int>(snapshots.size());
    auto parent_name = head_snapshot->get_parents_name();

    assert(num_snapshots <= snapshot_count && "can't have more snapshots than were ever taken");

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

auto BaseVirtualMachine::make_head_file_rollback(const Path& head_path, QFile& head_file) const
{
    return sg::make_scope_guard([this, &head_path, &head_file, old_head = head_snapshot->get_parents_name(),
                                 existed = head_file.exists()]() noexcept {
        head_file_rollback_helper(head_path, head_file, old_head, existed);
    });
}

void BaseVirtualMachine::head_file_rollback_helper(const Path& head_path, QFile& head_file, const std::string& old_head,
                                                   bool existed) const
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

    const auto snapshot_filename = derive_snapshot_filename(derive_index_string(snapshot_count),
                                                            QString::fromStdString(head_snapshot->get_name()));

    auto snapshot_filepath = snapshot_dir.filePath(snapshot_filename);
    auto head_path = derive_head_path(snapshot_dir);
    auto count_path = snapshot_dir.filePath(count_filename);

    auto rollback_snapshot_file = sg::make_scope_guard([&snapshot_filepath]() noexcept {
        QFile{snapshot_filepath}.remove(); // best effort, ignore return
    });

    mp::write_json(head_snapshot->serialize(), snapshot_filepath);

    QFile head_file{head_path};

    auto head_file_rollback = make_head_file_rollback(head_path, head_file);

    persist_head_snapshot_name(head_path);
    MP_UTILS.make_file_with_content(count_path.toStdString(), std::to_string(snapshot_count), yes_overwrite);

    rollback_snapshot_file.dismiss();
    head_file_rollback.dismiss();
}

void BaseVirtualMachine::persist_head_snapshot_name(const Path& head_path) const
{
    auto head_name = head_snapshot ? head_snapshot->get_name() : "";
    MP_UTILS.make_file_with_content(head_path.toStdString(), head_name, yes_overwrite);
}

std::string BaseVirtualMachine::generate_snapshot_name() const
{
    return fmt::format("snapshot{}", snapshot_count + 1);
}

auto BaseVirtualMachine::make_restore_rollback(const Path& head_path, VMSpecs& specs)
{
    return sg::make_scope_guard([this, &head_path, old_head = head_snapshot, old_specs = specs, &specs]() noexcept {
        top_catch_all(vm_name, &BaseVirtualMachine::restore_rollback_helper, this, head_path, old_head, old_specs,
                      specs);
    });
}

void BaseVirtualMachine::restore_rollback_helper(const Path& head_path, const std::shared_ptr<Snapshot>& old_head,
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
    std::unique_lock lock{snapshot_mutex};
    assert_vm_stopped(state); // precondition

    auto snapshot = get_snapshot(name);

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
