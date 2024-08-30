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

#include <multipass/cloud_init_iso.h>
#include <multipass/exceptions/file_open_failed_exception.h>
#include <multipass/exceptions/internal_timeout_exception.h>
#include <multipass/exceptions/ip_unavailable_exception.h>
#include <multipass/exceptions/snapshot_exceptions.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/snapshot.h>
#include <multipass/ssh/ssh_key_provider.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/top_catch_all.h>
#include <multipass/vm_specs.h>

#include <scope_guard.hpp>

#include <QDir>

#include <chrono>
#include <functional>
#include <mutex>
#include <stdexcept>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

using namespace std::chrono_literals;

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

std::string trimmed_contents_of(const QString& file_path)
{
    return mpu::trim(mpu::contents_of(file_path));
}

template <typename ExceptionT>
mp::utils::TimeoutAction log_and_retry(const ExceptionT& e,
                                       const mp::VirtualMachine* vm,
                                       mpl::Level log_level = mpl::Level::trace)
{
    assert(vm);
    mpl::log(log_level, vm->vm_name, e.what());
    return mp::utils::TimeoutAction::retry;
};

std::optional<mp::SSHSession> wait_until_ssh_up_helper(mp::VirtualMachine* virtual_machine,
                                                       std::chrono::milliseconds timeout,
                                                       const mp::SSHKeyProvider& key_provider)
{
    static constexpr auto wait_step = 1s;
    mpl::log(mpl::Level::debug, virtual_machine->vm_name, "Waiting for SSH to be up");

    std::optional<mp::SSHSession> session = std::nullopt;
    auto action = [virtual_machine, &key_provider, &session] {
        virtual_machine->ensure_vm_is_running();
        try
        {
            session.emplace(virtual_machine->ssh_hostname(wait_step),
                            virtual_machine->ssh_port(),
                            virtual_machine->ssh_username(),
                            key_provider);

            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            virtual_machine->state = mp::VirtualMachine::State::running;
            virtual_machine->update_state();
            return mp::utils::TimeoutAction::done;
        }
        catch (const mp::InternalTimeoutException& e)
        {
            return log_and_retry(e, virtual_machine);
        }
        catch (const mp::SSHException& e)
        {
            return log_and_retry(e, virtual_machine);
        }
        catch (const mp::IPUnavailableException& e)
        {
            return log_and_retry(e, virtual_machine);
        }
        catch (const std::runtime_error& e) // transitioning away from catching generic runtime errors
        {                                   // TODO remove once we're confident this is an anomaly
            return log_and_retry(e, virtual_machine, mpl::Level::warning);
        }
    };

    auto on_timeout = [virtual_machine] {
        std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
        virtual_machine->state = mp::VirtualMachine::State::unknown;
        virtual_machine->update_state();
        throw std::runtime_error(fmt::format("{}: timed out waiting for response", virtual_machine->vm_name));
    };

    mp::utils::try_action_for(on_timeout, timeout, action);
    return session;
}
} // namespace

mp::BaseVirtualMachine::BaseVirtualMachine(VirtualMachine::State state,
                                           const std::string& vm_name,
                                           const SSHKeyProvider& key_provider,
                                           const Path& instance_dir)
    : VirtualMachine{state, vm_name, instance_dir}, key_provider{key_provider}
{
}

mp::BaseVirtualMachine::BaseVirtualMachine(const std::string& vm_name,
                                           const SSHKeyProvider& key_provider,
                                           const Path& instance_dir)
    : VirtualMachine{vm_name, instance_dir}, key_provider{key_provider}
{
}

void mp::BaseVirtualMachine::apply_extra_interfaces_and_instance_id_to_cloud_init(
    const std::string& default_mac_addr,
    const std::vector<NetworkInterface>& extra_interfaces,
    const std::string& new_instance_id) const
{
    const std::filesystem::path cloud_init_config_iso_file_path =
        std::filesystem::path{instance_dir.absolutePath().toStdString()} / "cloud-init-config.iso";

    MP_CLOUD_INIT_FILE_OPS.update_cloud_init_with_new_extra_interfaces_and_new_id(default_mac_addr,
                                                                                  extra_interfaces,
                                                                                  new_instance_id,
                                                                                  cloud_init_config_iso_file_path);
}

void mp::BaseVirtualMachine::add_extra_interface_to_instance_cloud_init(const std::string& default_mac_addr,
                                                                        const NetworkInterface& extra_interface) const
{
    const std::filesystem::path cloud_init_config_iso_file_path =
        std::filesystem::path{instance_dir.absolutePath().toStdString()} / "cloud-init-config.iso";

    MP_CLOUD_INIT_FILE_OPS.add_extra_interface_to_cloud_init(default_mac_addr,
                                                             extra_interface,
                                                             cloud_init_config_iso_file_path);
}

std::string mp::BaseVirtualMachine::get_instance_id_from_the_cloud_init() const
{
    const std::filesystem::path cloud_init_config_iso_file_path =
        std::filesystem::path{instance_dir.absolutePath().toStdString()} / "cloud-init-config.iso";

    return MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(cloud_init_config_iso_file_path);
}

void mp::BaseVirtualMachine::check_state_for_shutdown(bool force)
{
    // A mutex should already be locked by the caller here
    if (state == State::off || state == State::stopped)
    {
        throw VMStateIdempotentException{"Ignoring shutdown since instance is already stopped."};
    }

    if (force)
    {
        return;
    }

    if (state == State::suspending)
    {
        throw VMStateInvalidException{"Cannot stop instance while suspending."};
    }

    if (state == State::suspended)
    {
        throw VMStateInvalidException{"Cannot stop suspended instance."};
    }

    if (state == State::starting || state == State::restarting)
    {
        throw VMStateInvalidException{"Cannot stop instance while starting."};
    }
}

std::string mp::BaseVirtualMachine::ssh_exec(const std::string& cmd, bool whisper)
{
    std::unique_lock lock{state_mutex};

    std::optional<std::string> log_details = std::nullopt;
    bool reconnect = true;
    while (true)
    {
        assert(reconnect && "we should have thrown otherwise");
        if ((!ssh_session || !ssh_session->is_connected()) && reconnect)
        {
            const auto msg =
                fmt::format("SSH session disconnected{}", log_details ? fmt::format(": {}", *log_details) : "");
            mpl::log(logging::Level::info, vm_name, msg);

            reconnect = false; // once only
            lock.unlock();
            renew_ssh_session();
            lock.lock();
        }

        try
        {
            return MP_UTILS.run_in_ssh_session(*ssh_session, cmd, whisper);
        }
        catch (const SSHException& e)
        {
            assert(ssh_session);
            if (ssh_session->is_connected() || !reconnect)
                throw;

            log_details = e.what();
            continue; // disconnections are often only detected after attempted use
        }
    }

    assert(false && "we should never reach here");
}

void mp::BaseVirtualMachine::renew_ssh_session()
{
    {
        const std::unique_lock lock{state_mutex};
        if (!MP_UTILS.is_running(current_state())) // spend time updating state only if we need a new session
            throw SSHException{fmt::format("SSH unavailable on instance {}: not running", vm_name)};
    }

    mpl::log(logging::Level::debug,
             vm_name,
             fmt::format("{} SSH session", ssh_session ? "Renewing cached" : "Caching new"));

    ssh_session.emplace(ssh_hostname(), ssh_port(), ssh_username(), key_provider);
}

void mp::BaseVirtualMachine::wait_until_ssh_up(std::chrono::milliseconds timeout)
{
    drop_ssh_session();
    ssh_session = wait_until_ssh_up_helper(this, timeout, key_provider);
    mpl::log(logging::Level::debug, vm_name, "Caching initial SSH session");
}

void mp::BaseVirtualMachine::wait_for_cloud_init(std::chrono::milliseconds timeout)
{
    auto action = [this] {
        ensure_vm_is_running();
        try
        {
            ssh_exec("[ -e /var/lib/cloud/instance/boot-finished ]");
            return mp::utils::TimeoutAction::done;
        }
        catch (const SSHExecFailure& e)
        {
            return mp::utils::TimeoutAction::retry;
        }
        catch (const std::exception& e) // transitioning away from catching generic runtime errors
        {                               // TODO remove once we're confident this is an anomaly
            mpl::log(mpl::Level::warning, vm_name, e.what());
            return mp::utils::TimeoutAction::retry;
        }
    };

    auto on_timeout = [] { throw std::runtime_error("timed out waiting for initialization to complete"); };
    mp::utils::try_action_for(on_timeout, timeout, action);
}

std::vector<std::string> mp::BaseVirtualMachine::get_all_ipv4()
{
    std::vector<std::string> all_ipv4;

    if (MP_UTILS.is_running(current_state()))
    {
        try
        {
            auto ip_a_output = QString::fromStdString(
                ssh_exec("ip -brief -family inet address show scope global", /* whisper = */ true));

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

auto mp::BaseVirtualMachine::view_snapshots() const -> SnapshotVista
{
    require_snapshots_support();
    SnapshotVista ret;

    const std::unique_lock lock{snapshot_mutex};
    ret.reserve(snapshots.size());
    std::transform(std::cbegin(snapshots), std::cend(snapshots), std::back_inserter(ret), [](const auto& pair) {
        return pair.second;
    });

    return ret;
}

std::shared_ptr<const mp::Snapshot> mp::BaseVirtualMachine::get_snapshot(const std::string& name) const
{
    require_snapshots_support();
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

std::shared_ptr<const mp::Snapshot> mp::BaseVirtualMachine::get_snapshot(int index) const
{
    require_snapshots_support();
    const std::unique_lock lock{snapshot_mutex};

    auto index_matcher = [index](const auto& elem) { return elem.second->get_index() == index; };
    if (auto it = std::find_if(snapshots.begin(), snapshots.end(), index_matcher); it != snapshots.end())
        return it->second;

    throw std::runtime_error{
        fmt::format("No snapshot with given index in instance; instance name: {}; snapshot index: {}", vm_name, index)};
}

std::shared_ptr<mp::Snapshot> mp::BaseVirtualMachine::get_snapshot(const std::string& name)
{
    return std::const_pointer_cast<Snapshot>(std::as_const(*this).get_snapshot(name));
}

std::shared_ptr<mp::Snapshot> mp::BaseVirtualMachine::get_snapshot(int index)
{
    return std::const_pointer_cast<Snapshot>(std::as_const(*this).get_snapshot(index));
}

void mp::BaseVirtualMachine::take_snapshot_rollback_helper(SnapshotMap::iterator it,
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

auto mp::BaseVirtualMachine::make_take_snapshot_rollback(SnapshotMap::iterator it)
{
    return sg::make_scope_guard( // best effort to rollback
        [this, it = it, old_head = head_snapshot, old_count = snapshot_count]() mutable noexcept {
            take_snapshot_rollback_helper(it, old_head, old_count);
        });
}

std::shared_ptr<const mp::Snapshot> mp::BaseVirtualMachine::take_snapshot(const VMSpecs& specs,
                                                                          const std::string& snapshot_name,
                                                                          const std::string& comment)
{
    require_snapshots_support();

    std::unique_lock lock{snapshot_mutex};
    assert_vm_stopped(state); // precondition

    auto sname = snapshot_name.empty() ? generate_snapshot_name() : snapshot_name;

    const auto [it, success] = snapshots.try_emplace(sname, nullptr);
    if (!success)
    {
        mpl::log(mpl::Level::warning, vm_name, fmt::format("Snapshot name taken: {}", sname));
        throw SnapshotNameTakenException{vm_name, sname};
    }

    auto rollback_on_failure = make_take_snapshot_rollback(it);

    // get instance id from cloud-init file or lxd cloud init config and pass to make_specific_snapshot
    auto ret = head_snapshot = it->second =
        make_specific_snapshot(sname, comment, get_instance_id_from_the_cloud_init(), specs, head_snapshot);
    ret->capture();

    ++snapshot_count;
    persist_generic_snapshot_info();

    rollback_on_failure.dismiss();
    log_latest_snapshot(std::move(lock));

    return ret;
}

bool mp::BaseVirtualMachine::updated_deleted_head(std::shared_ptr<Snapshot>& snapshot, const Path& head_path)
{
    if (head_snapshot == snapshot)
    {
        head_snapshot = snapshot->get_parent();
        persist_head_snapshot_index(head_path);
        return true;
    }

    return false;
}

auto mp::BaseVirtualMachine::make_deleted_head_rollback(const Path& head_path, const bool& wrote_head)
{
    return sg::make_scope_guard([this, old_head = head_snapshot, &head_path, &wrote_head]() mutable noexcept {
        deleted_head_rollback_helper(head_path, wrote_head, old_head);
    });
}

void mp::BaseVirtualMachine::deleted_head_rollback_helper(const Path& head_path,
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

auto mp::BaseVirtualMachine::make_parent_update_rollback(const std::shared_ptr<Snapshot>& deleted_parent,
                                                         std::vector<Snapshot*>& updated_parents) const
{
    return sg::make_scope_guard([this, &updated_parents, deleted_parent]() noexcept {
        top_catch_all(vm_name, &update_parents_rollback_helper, deleted_parent, updated_parents);
    });
}

void mp::BaseVirtualMachine::delete_snapshot_helper(std::shared_ptr<Snapshot>& snapshot)
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

void mp::BaseVirtualMachine::update_parents(std::shared_ptr<Snapshot>& deleted_parent,
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
auto mp::BaseVirtualMachine::make_reinsert_guard(NodeT& snapshot_node)
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

void mp::BaseVirtualMachine::rename_snapshot(const std::string& old_name, const std::string& new_name)
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

void mp::BaseVirtualMachine::delete_snapshot(const std::string& name)
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

void mp::BaseVirtualMachine::load_snapshots()
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

std::vector<std::string> mp::BaseVirtualMachine::get_childrens_names(const Snapshot* parent) const
{
    require_snapshots_support();
    std::vector<std::string> children;

    for (const auto& snapshot : view_snapshots())
        if (snapshot->get_parent().get() == parent)
            children.push_back(snapshot->get_name());

    return children;
}

void mp::BaseVirtualMachine::load_generic_snapshot_info()
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
void mp::BaseVirtualMachine::log_latest_snapshot(LockT lock) const
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

void mp::BaseVirtualMachine::load_snapshot(const QString& filename)
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

auto mp::BaseVirtualMachine::make_common_file_rollback(const Path& file_path,
                                                       QFile& file,
                                                       const std::string& old_contents) const
{
    return sg::make_scope_guard([this, &file_path, &file, old_contents, existed = file.exists()]() noexcept {
        common_file_rollback_helper(file_path, file, old_contents, existed);
    });
}

void mp::BaseVirtualMachine::common_file_rollback_helper(const Path& file_path,
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

void mp::BaseVirtualMachine::persist_generic_snapshot_info() const
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

void mp::BaseVirtualMachine::persist_head_snapshot_index(const Path& head_path) const
{
    auto head_index = head_snapshot ? head_snapshot->get_index() : 0;
    MP_UTILS.make_file_with_content(head_path.toStdString(), std::to_string(head_index) + "\n", yes_overwrite);
}

std::string mp::BaseVirtualMachine::generate_snapshot_name() const
{
    return fmt::format("snapshot{}", snapshot_count + 1);
}

auto mp::BaseVirtualMachine::make_restore_rollback(const Path& head_path, VMSpecs& specs)
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

void mp::BaseVirtualMachine::restore_rollback_helper(const Path& head_path,
                                                     const std::shared_ptr<Snapshot>& old_head,
                                                     const VMSpecs& old_specs,
                                                     VMSpecs& specs)
{
    // best effort only
    specs = old_specs;
    if (old_head != head_snapshot)
    {
        head_snapshot = old_head;
        persist_head_snapshot_index(head_path);
    }
}

void mp::BaseVirtualMachine::restore_snapshot(const std::string& name, VMSpecs& specs)
{
    const std::unique_lock lock{snapshot_mutex};

    auto snapshot = get_snapshot(name);

    assert_vm_stopped(state);                 // precondition
    assert_vm_stopped(snapshot->get_state()); // precondition

    const auto head_path = derive_head_path(instance_dir);
    auto rollback = make_restore_rollback(head_path, specs);

    specs.state = snapshot->get_state();
    specs.num_cores = snapshot->get_num_cores();
    specs.mem_size = snapshot->get_mem_size();
    specs.disk_space = snapshot->get_disk_space();
    const bool are_extra_interfaces_different = specs.extra_interfaces != snapshot->get_extra_interfaces();
    specs.extra_interfaces = snapshot->get_extra_interfaces();
    specs.mounts = snapshot->get_mounts();
    specs.metadata = snapshot->get_metadata();

    if (head_snapshot != snapshot)
    {
        head_snapshot = snapshot;
        persist_head_snapshot_index(head_path);
    }

    snapshot->apply();

    if (are_extra_interfaces_different)
    {
        // here we can use default_mac_address of the current state because it is an immutable variable.
        apply_extra_interfaces_and_instance_id_to_cloud_init(specs.default_mac_address,
                                                             snapshot->get_extra_interfaces(),
                                                             snapshot->get_cloud_init_instance_id());
    }

    rollback.dismiss();
}

std::shared_ptr<mp::Snapshot> mp::BaseVirtualMachine::make_specific_snapshot(const std::string& /*snapshot_name*/,
                                                                             const std::string& /*comment*/,
                                                                             const std::string& /*instance_id*/,
                                                                             const VMSpecs& /*specs*/,
                                                                             std::shared_ptr<Snapshot> /*parent*/)
{
    throw NotImplementedOnThisBackendException{"snapshots"};
}

std::shared_ptr<mp::Snapshot> mp::BaseVirtualMachine::make_specific_snapshot(const QString& /*filename*/)
{
    throw NotImplementedOnThisBackendException{"snapshots"};
}

void mp::BaseVirtualMachine::drop_ssh_session()
{
    if (ssh_session)
    {
        mpl::log(mpl::Level::debug, vm_name, "Dropping cached SSH session");
        ssh_session.reset();
    }
}
