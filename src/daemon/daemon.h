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

#pragma once

#include "daemon_rpc.h"
#include "readiness_result.h"

#include <multipass/async_periodic_download_task.h>
#include <multipass/delayed_shutdown_timer.h>
#include <multipass/format.h>
#include <multipass/mount_handler.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_specs.h>
#include <multipass/vm_status_monitor.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QFutureWatcher>

namespace multipass
{
struct DaemonConfig;
struct DaemonRpcContext;
class SettingsHandler;

// Guests recorded to be started again by the daemon that takes over after a
// snap refresh (they were running and were cleanly stopped to allow it).
struct RefreshRestartIntent
{
    std::unordered_set<std::string> names;
    // Revision of the daemon that wrote the intent. At startup, an intent
    // written by the same revision means the refresh did not happen (the
    // preparation was aborted) and must not start any guest.
    std::string daemon_revision;
    std::int64_t written_at{}; // unix seconds, informational
};

class Daemon : public QObject, public multipass::VMStatusMonitor
{
    Q_OBJECT
public:
    explicit Daemon(std::unique_ptr<const DaemonConfig> config);
    ~Daemon();

    void persist_instances();
    void shutdown_grpc_server();

protected:
    using InstanceTable = std::unordered_map<std::string, VirtualMachine::ShPtr>;

    // TODO hyperv migration, revert: back to a free function in daemon.cpp
    void connect_rpc(DaemonRpc& rpc);

    void on_resume() override;
    void on_shutdown() override;
    void on_suspend() override;
    void on_restart(const std::string& name) override;
    void persist_state_for(const std::string& name, const VirtualMachine::State& state) override;
    void update_metadata_for(const std::string& name, const boost::json::object& metadata) override;
    boost::json::object retrieve_metadata_for(const std::string& name) override;

public slots:
    virtual void create(const CreateRequest* request,
                        grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>* server,
                        DaemonRpcContext* context);

    virtual void launch(const LaunchRequest* request,
                        grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>* server,
                        DaemonRpcContext* context);

    virtual void purge(const PurgeRequest* request,
                       grpc::ServerReaderWriterInterface<PurgeReply, PurgeRequest>* server,
                       DaemonRpcContext* context);

    virtual void remotes(const RemotesRequest* request,
                         grpc::ServerReaderWriterInterface<RemotesReply, RemotesRequest>* server,
                         DaemonRpcContext* context);

    virtual void images(const ImagesRequest* request,
                        grpc::ServerReaderWriterInterface<ImagesReply, ImagesRequest>* server,
                        DaemonRpcContext* context);

    virtual void info(const InfoRequest* request,
                      grpc::ServerReaderWriterInterface<InfoReply, InfoRequest>* server,
                      DaemonRpcContext* context);

    virtual void list(const ListRequest* request,
                      grpc::ServerReaderWriterInterface<ListReply, ListRequest>* server,
                      DaemonRpcContext* context);

    virtual void snapshots(
        const SnapshotsRequest* request,
        grpc::ServerReaderWriterInterface<SnapshotsReply, SnapshotsRequest>* server,
        DaemonRpcContext* context);

    virtual void networks(const NetworksRequest* request,
                          grpc::ServerReaderWriterInterface<NetworksReply, NetworksRequest>* server,
                          DaemonRpcContext* context);

    virtual void mount(const MountRequest* request,
                       grpc::ServerReaderWriterInterface<MountReply, MountRequest>* server,
                       DaemonRpcContext* context);

    virtual void recover(const RecoverRequest* request,
                         grpc::ServerReaderWriterInterface<RecoverReply, RecoverRequest>* server,
                         DaemonRpcContext* context);

    virtual void ssh_info(const SSHInfoRequest* request,
                          grpc::ServerReaderWriterInterface<SSHInfoReply, SSHInfoRequest>* server,
                          DaemonRpcContext* context);

    virtual void start(const StartRequest* request,
                       grpc::ServerReaderWriterInterface<StartReply, StartRequest>* server,
                       DaemonRpcContext* context);

    virtual void stop(const StopRequest* request,
                      grpc::ServerReaderWriterInterface<StopReply, StopRequest>* server,
                      DaemonRpcContext* context);

    virtual void suspend(const SuspendRequest* request,
                         grpc::ServerReaderWriterInterface<SuspendReply, SuspendRequest>* server,
                         DaemonRpcContext* context);

    virtual void restart(const RestartRequest* request,
                         grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>* server,
                         DaemonRpcContext* context);

    virtual void delet(const DeleteRequest* request,
                       grpc::ServerReaderWriterInterface<DeleteReply, DeleteRequest>* server,
                       DaemonRpcContext* context);

    virtual void umount(const UmountRequest* request,
                        grpc::ServerReaderWriterInterface<UmountReply, UmountRequest>* server,
                        DaemonRpcContext* context);

    virtual void version(const VersionRequest* request,
                         grpc::ServerReaderWriterInterface<VersionReply, VersionRequest>* server,
                         DaemonRpcContext* context);

    virtual void get(const GetRequest* request,
                     grpc::ServerReaderWriterInterface<GetReply, GetRequest>* server,
                     DaemonRpcContext* context);

    virtual void set(const SetRequest* request,
                     grpc::ServerReaderWriterInterface<SetReply, SetRequest>* server,
                     DaemonRpcContext* context);

    virtual void keys(const KeysRequest* request,
                      grpc::ServerReaderWriterInterface<KeysReply, KeysRequest>* server,
                      DaemonRpcContext* context);

    virtual void authenticate(
        const AuthenticateRequest* request,
        grpc::ServerReaderWriterInterface<AuthenticateReply, AuthenticateRequest>* server,
        DaemonRpcContext* context);
    virtual void clone(const CloneRequest* request,
                       grpc::ServerReaderWriterInterface<CloneReply, CloneRequest>* server,
                       DaemonRpcContext* context);

    virtual void snapshot(const SnapshotRequest* request,
                          grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>* server,
                          DaemonRpcContext* context);

    virtual void restore(const RestoreRequest* request,
                         grpc::ServerReaderWriterInterface<RestoreReply, RestoreRequest>* server,
                         DaemonRpcContext* context);

    virtual void daemon_info(
        const DaemonInfoRequest* request,
        grpc::ServerReaderWriterInterface<DaemonInfoReply, DaemonInfoRequest>* server,
        DaemonRpcContext* context);

    virtual void zones(const ZonesRequest* request,
                       grpc::ServerReaderWriterInterface<ZonesReply, ZonesRequest>* server,
                       DaemonRpcContext* context);

    virtual void zones_state(
        const ZonesStateRequest* request,
        grpc::ServerReaderWriterInterface<ZonesStateReply, ZonesStateRequest>* server,
        DaemonRpcContext* context);

    virtual void prepare_refresh(
        const PrepareRefreshRequest* request,
        grpc::ServerReaderWriterInterface<PrepareRefreshReply, PrepareRefreshRequest>* server,
        DaemonRpcContext* context);

    virtual void abort_refresh(
        const AbortRefreshRequest* request,
        grpc::ServerReaderWriterInterface<AbortRefreshReply, AbortRefreshRequest>* server,
        DaemonRpcContext* context);

    virtual void wait_ready(
        const WaitReadyRequest* request,
        grpc::ServerReaderWriterInterface<WaitReadyReply, WaitReadyRequest>* server,
        DaemonRpcContext* context);

private:
    // TODO hyperv migration, remove
    // Used at RPC dispatch.
    [[nodiscard]] bool reject_if_migrating(std::string_view rpc_name,
                                           DaemonRpcContext* context) const;

    void release_resources(const std::string& instance);
    void create_vm(const CreateRequest* request,
                   grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>* server,
                   DaemonRpcContext* context,
                   bool start);
    void delete_vm(InstanceTable::iterator vm_it, DeleteReply& response);
    grpc::Status reboot_vm(VirtualMachine& vm, bool running_only);
    grpc::Status shutdown_vm(VirtualMachine& vm, const std::chrono::milliseconds delay);
    grpc::Status switch_off_vm(VirtualMachine& vm);
    grpc::Status make_vm_unavailable(VirtualMachine& vm);
    grpc::Status cancel_vm_shutdown(const VirtualMachine& vm);
    grpc::Status get_ssh_info_for_vm(VirtualMachine& vm, SSHInfoReply& response);
    void snapshot_finish(const std::string& instance_name,
                         const SnapshotRequest& request,
                         bool restart,
                         grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>* server,
                         DaemonRpcContext* context);
    grpc::Status snapshot_shutdown_vm(
        VirtualMachine& vm,
        grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>* server);
    void snapshot_restart_vm(
        VirtualMachine& vm,
        SnapshotReply& final_reply,
        DaemonRpcContext* context,
        grpc::Status& final_status,
        grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>* server);

    // Starts the suspended targets (main thread only); returns the VMs that were started
    grpc::Status restart_start_suspended_instances(
        const std::vector<InstanceTable::iterator>& instance_targets,
        std::vector<VirtualMachine::ShPtr>& started_vms);

    void init_mounts(const std::string& name);
    void stop_mounts(const std::string& name);

    // This returns whether any specs were updated (and need persisting)
    bool update_mounts(VMSpecs& vm_specs,
                       std::unordered_map<std::string, MountHandler::UPtr>& vm_mounts,
                       VirtualMachine* vm);

    // This returns whether all required mount handlers were successfully created
    bool create_missing_mounts(std::unordered_map<std::string, VMMount>& mount_specs,
                               std::unordered_map<std::string, MountHandler::UPtr>& vm_mounts,
                               VirtualMachine* vm);

    MountHandler::UPtr make_mount(VirtualMachine* vm,
                                  const std::string& target,
                                  const VMMount& mount);

    struct AsyncOperationStatus
    {
        grpc::Status status;
        DaemonRpcContext* context;
    };

    // These async_* methods need to operate on instance names and look up the VMs again, lest they
    // be gone or moved.
    template <typename Reply, typename Request>
    ReadinessResult async_wait_vm_ready(const std::string& name,
                                        const std::chrono::seconds& timeout,
                                        grpc::ServerReaderWriterInterface<Reply, Request>* server);
    template <typename Reply, typename Request>
    AsyncOperationStatus
    async_wait_multiple_vms_ready(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                                  const std::vector<std::string>& vms,
                                  const std::chrono::seconds& timeout,
                                  DaemonRpcContext* context,
                                  const std::string& errors);
    void finish_async_operation(const std::string& async_future_key);
    QFutureWatcher<AsyncOperationStatus>* create_future_watcher(
        std::function<void()> const& finished_op = []() {});
    void update_manifests_all(const bool force_update = false);
    void wait_update_manifests_all_and_optionally_applied_force(
        const bool force_manifest_network_download);

    template <typename Reply, typename Request>
    void reply_msg(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                   std::string&& msg,
                   bool sticky = false);

    void populate_instance_info(VirtualMachine& vm,
                                InfoReply& response,
                                bool runtime_info,
                                bool deleted,
                                bool& have_mounts);

    std::string dest_name_for_clone(const CloneRequest& request);
    grpc::Status validate_dest_name(const std::string& name);
    VMSpecs clone_spec(const VMSpecs& src_vm_spec,
                       const std::string& src_name,
                       const std::string& dest_name);

    std::unique_ptr<const DaemonConfig> config;

protected:
    std::unordered_map<std::string, VMSpecs> vm_instance_specs;
    InstanceTable operative_instances;

    // TODO hyperv migration, remove
    // Set only while a bulk Hyper-V -> HCS migration runs inside Daemon::set. It guards
    // conflicting mutating RPCs without taking a lock, so it cannot deadlock against the
    // long-running migration. Atomic because RPC slots and the migration run on different
    // threads.
    std::atomic<bool> migration_in_progress{false};

    bool is_bridged(const std::string& instance_name) const;
    void add_bridged_interface(const std::string& instance_name);

private:
    InstanceTable deleted_instances;
    std::unordered_map<std::string, std::unique_ptr<DelayedShutdownTimer>>
        delayed_shutdown_instances;
    std::unordered_set<std::string> allocated_mac_addrs;
    DaemonRpc daemon_rpc;
    QTimer source_images_maintenance_task;
    multipass::utils::AsyncPeriodicDownloadTask<void> update_manifests_all_task{
        "fetch manifest periodically",
        std::chrono::minutes(15),
        std::chrono::seconds(5),
        &Daemon::update_manifests_all,
        this,
        false};
    std::unordered_map<std::string, std::unique_ptr<QFutureWatcher<AsyncOperationStatus>>>
        async_future_watchers;
    std::unordered_map<std::string, QFuture<ReadinessResult>> async_running_futures;
    std::mutex start_mutex;
    std::unordered_set<std::string> preparing_instances;
    QFuture<void> image_update_future;
    SettingsHandler* instance_mod_handler;
    SettingsHandler* snapshot_mod_handler;
    std::unordered_map<std::string, std::unordered_map<std::string, MountHandler::UPtr>> mounts;
    std::unordered_set<std::string> user_authorized_bridges;

    // Snap refresh preparation (see Daemon::prepare_refresh). The gate is
    // in-memory on purpose: it must never survive a daemon crash or restart,
    // so a failed or interrupted refresh can never leave a permanent lock.
    mutable std::mutex refresh_mutex;
    bool refresh_preparation_active{false};
    bool refresh_preparation_complete{false};
    std::chrono::steady_clock::time_point refresh_preparation_start{};
    std::vector<std::string> refresh_preparation_stopped;
    QTimer refresh_abort_timer;

    // Snap refresh preparation helpers
    [[nodiscard]] bool reject_if_refresh_preparing(std::string_view rpc_name,
                                                   DaemonRpcContext* context) const;
    QString refresh_intent_file_path() const;
    RefreshRestartIntent load_refresh_restart_intent() const;
    void save_refresh_restart_intent(const std::unordered_set<std::string>& names) const;
    void clear_refresh_restart_intent() const;
    void remove_from_refresh_intent(const std::string& name);
    // An empty list means "all operative instances" (e.g. `multipass stop`).
    void remove_refresh_intent_for(
        const google::protobuf::RepeatedPtrField<std::string>& instance_names);
    // Releases the gate and best-effort restores the given guests (used by
    // abort_refresh and by the post-preparation abort timer).
    void restore_guests_after_refresh_abort(const std::unordered_set<std::string>& to_restart);
    // If a prepared refresh did not proceed (snapd failed after the hook), the
    // gate expiry timer restores guests stopped for it.
    void on_refresh_abort_timeout();
    // Returns the instance's disk image if it carries a "suspend" memory
    // snapshot, or an empty path otherwise.
    std::filesystem::path instance_image_with_suspend_snapshot(const std::string& name) const;
};
} // namespace multipass
