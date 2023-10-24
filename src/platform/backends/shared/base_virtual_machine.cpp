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
#include <multipass/logging/log.h>
#include <multipass/snapshot.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>

#include <scope_guard.hpp>

#include <QDir>

#include <regex>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
using St = mp::VirtualMachine::State;
constexpr auto snapshot_extension = "snapshot.json";
constexpr auto head_filename = "snapshot-head";
constexpr auto count_filename = "snapshot-count";
constexpr auto yes_overwrite = true;

void assert_vm_stopped(St state)
{
    assert(state == St::off || state == St::stopped);
}

mp::Path derive_head_path(const QDir& snapshot_dir)
{
    return snapshot_dir.filePath(head_filename);
}

void update_parents_rollback_helper(const std::shared_ptr<mp::Snapshot>& deleted_parent,
                                    std::vector<mp::Snapshot*>& updated_parents)
{
    for (auto snapshot : updated_parents)
        snapshot->set_parent(deleted_parent);
}

std::string trim(const std::string& s)
{
    return std::regex_replace(s, std::regex{R"(^\s+|\s+$)"}, "");
}

std::string trimmed_contents_of(const QString& file_path)
{
    return trim(mpu::contents_of(file_path));
}
} // namespace

namespace multipass
{

BaseVirtualMachine::BaseVirtualMachine(VirtualMachine::State state,
                                       const std::string& vm_name,
                                       const mp::Path& instance_dir)
    : VirtualMachine(state, vm_name, instance_dir){};

BaseVirtualMachine::BaseVirtualMachine(const std::string& vm_name, const mp::Path& instance_dir)
    : VirtualMachine(vm_name, instance_dir){};

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
    std::transform(std::cbegin(snapshots), std::cend(snapshots), std::back_inserter(ret), [](const auto& pair) {
        return pair.second;
    });

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
        throw NoSuchSnapshotException{vm_name, name};
    }
}

std::shared_ptr<const Snapshot> BaseVirtualMachine::get_snapshot(int index) const
{
    const std::unique_lock lock{snapshot_mutex};

    auto index_matcher = [index](const auto& elem) { return elem.second->get_index() == index; };
    if (auto it = std::find_if(snapshots.begin(), snapshots.end(), index_matcher); it != snapshots.end())
        return it->second;

    throw std::runtime_error{
        fmt::format("No snapshot with given index in instance; instance name: {}; snapshot index: {}", vm_name, index)};
}

std::shared_ptr<Snapshot> BaseVirtualMachine::get_snapshot(const std::string& name)
{
    return std::const_pointer_cast<Snapshot>(std::as_const(*this).get_snapshot(name));
}

std::shared_ptr<Snapshot> BaseVirtualMachine::get_snapshot(int index)
{
    return std::const_pointer_cast<Snapshot>(std::as_const(*this).get_snapshot(index));
}

void BaseVirtualMachine::take_snapshot_rollback_helper(SnapshotMap::iterator it,
                                                       std::shared_ptr<Snapshot>& old_head,
                                                       int old_count)
{
    if (old_head != head_snapshot)
    {
        assert(it->second && "snapshot not created despite modified head");
        if (snapshot_count > old_count) // snapshot was captured
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

std::shared_ptr<const Snapshot> BaseVirtualMachine::take_snapshot(const VMSpecs& specs,
                                                                  const std::string& snapshot_name,
                                                                  const std::string& comment)
{
    std::string sname;

    {
        std::unique_lock lock{snapshot_mutex};
        assert_vm_stopped(state); // precondition

        sname = snapshot_name.empty() ? generate_snapshot_name() : snapshot_name;

        const auto [it, success] = snapshots.try_emplace(sname, nullptr);
        if (!success)
        {
            mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", sname));
            throw SnapshotNameTakenException{vm_name, sname};
        }

        auto rollback_on_failure = make_take_snapshot_rollback(it);

        auto ret = head_snapshot = it->second = make_specific_snapshot(sname, comment, specs, head_snapshot);
        ret->capture();

        ++snapshot_count;
        persist_generic_snapshot_info();

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
        persist_head_snapshot_index(head_path);
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

void BaseVirtualMachine::deleted_head_rollback_helper(const Path& head_path,
                                                      const bool& wrote_head,
                                                      std::shared_ptr<Snapshot>& old_head)
{
    if (head_snapshot != old_head)
    {
        head_snapshot = std::move(old_head);
        if (wrote_head)
            top_catch_all(vm_name, [this, &head_path] {
                MP_UTILS.make_file_with_content(head_path.toStdString(),
                                                std::to_string(head_snapshot->get_index()) + "\n",
                                                yes_overwrite);
            });
    }
}

auto BaseVirtualMachine::make_parent_update_rollback(const std::shared_ptr<Snapshot>& deleted_parent,
                                                     std::vector<Snapshot*>& updated_parents) const
{
    return sg::make_scope_guard([this, &updated_parents, deleted_parent]() noexcept {
        top_catch_all(vm_name, &update_parents_rollback_helper, deleted_parent, updated_parents);
    });
}

void BaseVirtualMachine::delete_snapshot_helper(std::shared_ptr<Snapshot>& snapshot)
{
    // Update head if deleted
    auto wrote_head = false;
    auto head_path = derive_head_path(instance_dir);
    auto rollback_head = make_deleted_head_rollback(head_path, wrote_head);
    wrote_head = updated_deleted_head(snapshot, head_path);

    // Update children of deleted snapshot
    std::vector<Snapshot*> updated_parents{};
    updated_parents.reserve(snapshots.size());

    auto rollback_parent_updates = make_parent_update_rollback(snapshot, updated_parents);
    update_parents(snapshot, updated_parents);

    // Erase the snapshot with the backend and dismiss rollbacks on success
    snapshot->erase();
    rollback_parent_updates.dismiss();
    rollback_head.dismiss();
}

void BaseVirtualMachine::update_parents(std::shared_ptr<Snapshot>& deleted_parent,
                                        std::vector<Snapshot*>& updated_parents)
{
    auto new_parent = deleted_parent->get_parent();
    for (auto& [ignore, other] : snapshots)
    {
        if (other->get_parent() == deleted_parent)
        {
            other->set_parent(new_parent);
            updated_parents.push_back(other.get());
        }
    }
}

template <typename NodeT>
auto BaseVirtualMachine::make_reinsert_guard(NodeT& snapshot_node)
{
    return sg::make_scope_guard([this, &snapshot_node]() noexcept {
        top_catch_all(vm_name, [this, &snapshot_node] {
            const auto& current_name = snapshot_node.mapped()->get_name();
            if (auto& key = snapshot_node.key(); key != current_name)
                key = current_name; // best-effort rollback (this is very unlikely to fail)

            snapshots.insert(std::move(snapshot_node));
        });
    });
    ;
}

void BaseVirtualMachine::rename_snapshot(const std::string& old_name, const std::string& new_name)
{
    if (old_name == new_name)
        return;

    const std::unique_lock lock{snapshot_mutex};

    auto old_it = snapshots.find(old_name);
    if (old_it == snapshots.end())
        throw NoSuchSnapshotException{vm_name, old_name};

    if (snapshots.find(new_name) != snapshots.end())
        throw SnapshotNameTakenException{vm_name, new_name};

    auto snapshot_node = snapshots.extract(old_it);
    const auto reinsert_guard = make_reinsert_guard(snapshot_node); // we want this to execute both on failure & success

    snapshot_node.key() = new_name;
    snapshot_node.mapped()->set_name(new_name);
}

void BaseVirtualMachine::delete_snapshot(const std::string& name)
{
    const std::unique_lock lock{snapshot_mutex};

    auto it = snapshots.find(name);
    if (it == snapshots.end())
        throw NoSuchSnapshotException{vm_name, name};

    auto snapshot = it->second;
    delete_snapshot_helper(snapshot);

    snapshots.erase(it); // doesn't throw
    mpl::log(mpl::Level::debug, vm_name, fmt::format("Snapshot deleted: {}", name));
}

void BaseVirtualMachine::load_snapshots()
{
    const std::unique_lock lock{snapshot_mutex};

    auto snapshot_files = MP_FILEOPS.entryInfoList(instance_dir,
                                                   {QString{"*.%1"}.arg(snapshot_extension)},
                                                   QDir::Filter::Files | QDir::Filter::Readable,
                                                   QDir::SortFlag::Name);
    for (const auto& finfo : snapshot_files)
        load_snapshot(finfo.filePath());

    load_generic_snapshot_info();
}

std::vector<std::string> BaseVirtualMachine::get_childrens_names(const Snapshot* parent) const
{
    std::vector<std::string> children;

    for (const auto& snapshot : view_snapshots())
        if (snapshot->get_parent().get() == parent)
            children.push_back(snapshot->get_name());

    return children;
}

void BaseVirtualMachine::load_generic_snapshot_info()
{
    try
    {
        snapshot_count = std::stoi(trimmed_contents_of(instance_dir.filePath(count_filename)));

        auto head_index = std::stoi(trimmed_contents_of(instance_dir.filePath(head_filename)));
        head_snapshot = head_index ? get_snapshot(head_index) : nullptr;
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

        mpl::log(log_detail_lvl,
                 vm_name,
                 fmt::format(R"(New snapshot: "{}"; Descendant of: "{}"; Total snapshots: {})",
                             name,
                             parent_name,
                             num_snapshots));
    }
}

void BaseVirtualMachine::load_snapshot(const QString& filename)
{
    auto snapshot = make_specific_snapshot(filename);
    const auto& name = snapshot->get_name();
    auto [it, success] = snapshots.try_emplace(name, snapshot);

    if (!success)
    {
        mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", name));
        throw SnapshotNameTakenException{vm_name, name};
    }
}

auto BaseVirtualMachine::make_common_file_rollback(const Path& file_path,
                                                   QFile& file,
                                                   const std::string& old_contents) const
{
    return sg::make_scope_guard([this, &file_path, &file, old_contents, existed = file.exists()]() noexcept {
        common_file_rollback_helper(file_path, file, old_contents, existed);
    });
}

void BaseVirtualMachine::common_file_rollback_helper(const Path& file_path,
                                                     QFile& file,
                                                     const std::string& old_contents,
                                                     bool existed) const
{
    // best effort, ignore returns
    if (!existed)
        file.remove();
    else
        top_catch_all(vm_name, [&file_path, &old_contents] {
            MP_UTILS.make_file_with_content(file_path.toStdString(), old_contents, yes_overwrite);
        });
}

void BaseVirtualMachine::persist_generic_snapshot_info() const
{
    assert(head_snapshot);

    auto head_path = derive_head_path(instance_dir);
    auto count_path = instance_dir.filePath(count_filename);

    QFile head_file{head_path};
    auto head_file_rollback =
        make_common_file_rollback(head_path, head_file, std::to_string(head_snapshot->get_parents_index()) + "\n");
    persist_head_snapshot_index(head_path);

    QFile count_file{count_path};
    auto count_file_rollback =
        make_common_file_rollback(count_path, count_file, std::to_string(snapshot_count - 1) + "\n");
    MP_UTILS.make_file_with_content(count_path.toStdString(), std::to_string(snapshot_count) + "\n", yes_overwrite);

    count_file_rollback.dismiss();
    head_file_rollback.dismiss();
}

void BaseVirtualMachine::persist_head_snapshot_index(const Path& head_path) const
{
    auto head_index = head_snapshot ? head_snapshot->get_index() : 0;
    MP_UTILS.make_file_with_content(head_path.toStdString(), std::to_string(head_index) + "\n", yes_overwrite);
}

std::string BaseVirtualMachine::generate_snapshot_name() const
{
    return fmt::format("snapshot{}", snapshot_count + 1);
}

auto BaseVirtualMachine::make_restore_rollback(const Path& head_path, VMSpecs& specs)
{
    return sg::make_scope_guard([this, &head_path, old_head = head_snapshot, old_specs = specs, &specs]() noexcept {
        top_catch_all(vm_name,
                      &BaseVirtualMachine::restore_rollback_helper,
                      this,
                      head_path,
                      old_head,
                      old_specs,
                      specs);
    });
}

void BaseVirtualMachine::restore_rollback_helper(const Path& head_path,
                                                 const std::shared_ptr<Snapshot>& old_head,
                                                 const VMSpecs& old_specs,
                                                 VMSpecs& specs)
{
    // best effort only
    old_head->apply();
    specs = old_specs;
    if (old_head != head_snapshot)
    {
        head_snapshot = old_head;
        persist_head_snapshot_index(head_path);
    }
}

void BaseVirtualMachine::restore_snapshot(const std::string& name, VMSpecs& specs)
{
    const std::unique_lock lock{snapshot_mutex};
    assert_vm_stopped(state); // precondition

    auto snapshot = get_snapshot(name);

    // TODO@snapshots convert into runtime_error (persisted info could have been tampered with)
    assert(snapshot->get_state() == St::off || snapshot->get_state() == St::stopped);

    snapshot->apply();

    const auto head_path = derive_head_path(instance_dir);
    auto rollback = make_restore_rollback(head_path, specs);

    specs.state = snapshot->get_state();
    specs.num_cores = snapshot->get_num_cores();
    specs.mem_size = snapshot->get_mem_size();
    specs.disk_space = snapshot->get_disk_space();
    specs.mounts = snapshot->get_mounts();
    specs.metadata = snapshot->get_metadata();

    if (head_snapshot != snapshot)
    {
        head_snapshot = snapshot;
        persist_head_snapshot_index(head_path);
    }

    rollback.dismiss();
}

} // namespace multipass
