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

#ifndef MULTIPASS_BASE_VIRTUAL_MACHINE_H
#define MULTIPASS_BASE_VIRTUAL_MACHINE_H

#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/logging/log.h>
#include <multipass/path.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <fmt/format.h>

#include <QRegularExpression>
#include <QString>

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace multipass
{
class SSHSession;
class SSHKeyProvider;

class BaseVirtualMachine : public VirtualMachine
{
public:
    BaseVirtualMachine(VirtualMachine::State state,
                       const std::string& vm_name,
                       const SSHKeyProvider& key_provider,
                       const Path& instance_dir);
    BaseVirtualMachine(const std::string& vm_name, const SSHKeyProvider& key_provider, const Path& instance_dir);

    virtual std::string ssh_exec(const std::string& cmd, bool whisper = false) override;

    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;
    void wait_for_cloud_init(std::chrono::milliseconds timeout) override;

    std::vector<std::string> get_all_ipv4() override;
    void add_network_interface(int index,
                               const std::string& default_mac_addr,
                               const NetworkInterface& extra_interface) override
    {
        throw NotImplementedOnThisBackendException("networks");
    }
    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string& target, const VMMount& mount) override
    {
        throw NotImplementedOnThisBackendException("native mounts");
    };

    SnapshotVista view_snapshots() const override;
    int get_num_snapshots() const override;

    std::shared_ptr<const Snapshot> get_snapshot(const std::string& name) const override;
    std::shared_ptr<const Snapshot> get_snapshot(int index) const override;
    std::shared_ptr<Snapshot> get_snapshot(const std::string& name) override;
    std::shared_ptr<Snapshot> get_snapshot(int index) override;

    // TODO: the VM should know its directory, but that is true of everything in its VMDescription; pulling that from
    // derived classes is a big refactor
    std::shared_ptr<const Snapshot> take_snapshot(const VMSpecs& specs,
                                                  const std::string& snapshot_name,
                                                  const std::string& comment) override;
    void rename_snapshot(const std::string& old_name, const std::string& new_name) override;
    void delete_snapshot(const std::string& name) override;
    void restore_snapshot(const std::string& name, VMSpecs& specs) override;
    void load_snapshots() override;
    void load_snapshots_and_update_unique_identifiers(const VMSpecs& src_specs,
                                                      const VMSpecs& dest_specs,
                                                      const std::string& src_vm_name) override;
    std::vector<std::string> get_childrens_names(const Snapshot* parent) const override;
    int get_snapshot_count() const override;

protected:
    virtual void require_snapshots_support() const;
    virtual std::shared_ptr<Snapshot> make_specific_snapshot(const QString& filename);
    virtual std::shared_ptr<Snapshot> make_specific_snapshot(const std::string& snapshot_name,
                                                             const std::string& comment,
                                                             const std::string& instance_id,
                                                             const VMSpecs& specs,
                                                             std::shared_ptr<Snapshot> parent);
    virtual void drop_ssh_session(); // virtual to allow mocking
    void renew_ssh_session();
    virtual std::shared_ptr<Snapshot> make_specific_snapshot(const QString& filename,
                                                             const VMSpecs& src_specs,
                                                             const VMSpecs& dest_specs,
                                                             const std::string& src_vm_name);

    virtual void add_extra_interface_to_instance_cloud_init(const std::string& default_mac_addr,
                                                            const NetworkInterface& extra_interface) const;
    virtual void apply_extra_interfaces_and_instance_id_to_cloud_init(
        const std::string& default_mac_addr,
        const std::vector<NetworkInterface>& extra_interfaces,
        const std::string& new_instance_id) const;
    virtual std::string get_instance_id_from_the_cloud_init() const;

    virtual void check_state_for_shutdown(bool force);

private:
    using SnapshotMap = std::unordered_map<std::string, std::shared_ptr<Snapshot>>;

    void load_snapshots_common(const std::function<void(const QString&)>& load_function);
    template <typename LockT>
    void log_latest_snapshot(LockT lock) const;

    void load_generic_snapshot_info();

    template <typename... Args>
    void load_snapshot_and_optionally_update_unique_identifiers(const QString& file_path, Args&&... args);
    auto make_take_snapshot_rollback(SnapshotMap::iterator it);
    void take_snapshot_rollback_helper(SnapshotMap::iterator it, std::shared_ptr<Snapshot>& old_head, int old_count);

    auto make_common_file_rollback(const Path& file_path, QFile& file, const std::string& old_contents) const;
    void common_file_rollback_helper(const Path& file_path,
                                     QFile& file,
                                     const std::string& old_contents,
                                     bool existed) const;

    void persist_generic_snapshot_info() const;
    void persist_head_snapshot_index(const Path& head_path) const;
    std::string generate_snapshot_name() const;

    template <typename NodeT>
    auto make_reinsert_guard(NodeT& snapshot_node);

    auto make_restore_rollback(const Path& head_path, VMSpecs& specs);
    void restore_rollback_helper(const Path& head_path,
                                 const std::shared_ptr<Snapshot>& old_head,
                                 const VMSpecs& old_specs,
                                 VMSpecs& specs);

    bool updated_deleted_head(std::shared_ptr<Snapshot>& snapshot, const Path& head_path);
    auto make_deleted_head_rollback(const Path& head_path, const bool& wrote_head);
    void deleted_head_rollback_helper(const Path& head_path,
                                      const bool& wrote_head,
                                      std::shared_ptr<Snapshot>& old_head);

    void update_parents(std::shared_ptr<Snapshot>& deleted_parent, std::vector<Snapshot*>& updated_parents);
    auto make_parent_update_rollback(const std::shared_ptr<Snapshot>& deleted_parent,
                                     std::vector<Snapshot*>& updated_snapshot_paths) const;

    void delete_snapshot_helper(std::shared_ptr<Snapshot>& snapshot);

protected:
    const SSHKeyProvider& key_provider;

private:
    std::optional<SSHSession> ssh_session = std::nullopt;
    SnapshotMap snapshots;
    std::shared_ptr<Snapshot> head_snapshot = nullptr;
    int snapshot_count = 0; // tracks the number of snapshots ever taken (regardless or deletes)
    mutable std::recursive_mutex snapshot_mutex;
};

} // namespace multipass

inline int multipass::BaseVirtualMachine::get_num_snapshots() const
{
    require_snapshots_support();
    return static_cast<int>(snapshots.size());
}

inline int multipass::BaseVirtualMachine::get_snapshot_count() const
{
    require_snapshots_support();
    const std::unique_lock lock{snapshot_mutex};
    return snapshot_count;
}

inline void multipass::BaseVirtualMachine::require_snapshots_support() const
{
    throw NotImplementedOnThisBackendException{"snapshots"};
}

#endif // MULTIPASS_BASE_VIRTUAL_MACHINE_H
