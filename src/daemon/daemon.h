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

#ifndef MULTIPASS_DAEMON_H
#define MULTIPASS_DAEMON_H

#include "daemon_config.h"
#include "daemon_rpc.h"

#include <multipass/async_periodic_download_task.h>
#include <multipass/delayed_shutdown_timer.h>
#include <multipass/format.h>
#include <multipass/mount_handler.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_specs.h>
#include <multipass/vm_status_monitor.h>

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QFutureWatcher>

namespace multipass
{
struct DaemonConfig;
class SettingsHandler;

class Daemon : public QObject, public multipass::VMStatusMonitor
{
    Q_OBJECT
public:
    explicit Daemon(std::unique_ptr<const DaemonConfig> config);
    ~Daemon();

    void persist_instances();

protected:
    using InstanceTable = std::unordered_map<std::string, VirtualMachine::ShPtr>;

    void on_resume() override;
    void on_shutdown() override;
    void on_suspend() override;
    void on_restart(const std::string& name) override;
    void persist_state_for(const std::string& name, const VirtualMachine::State& state) override;
    void update_metadata_for(const std::string& name, const QJsonObject& metadata) override;
    QJsonObject retrieve_metadata_for(const std::string& name) override;

public slots:
    virtual void shutdown_grpc_server();

    virtual void create(const CreateRequest* request,
                        grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>* server,
                        std::promise<grpc::Status>* status_promise);

    virtual void launch(const LaunchRequest* request,
                        grpc::ServerReaderWriterInterface<LaunchReply, LaunchRequest>* server,
                        std::promise<grpc::Status>* status_promise);

    virtual void purge(const PurgeRequest* request, grpc::ServerReaderWriterInterface<PurgeReply, PurgeRequest>* server,
                       std::promise<grpc::Status>* status_promise);

    virtual void find(const FindRequest* request, grpc::ServerReaderWriterInterface<FindReply, FindRequest>* server,
                      std::promise<grpc::Status>* status_promise);

    virtual void info(const InfoRequest* request, grpc::ServerReaderWriterInterface<InfoReply, InfoRequest>* server,
                      std::promise<grpc::Status>* status_promise);

    virtual void list(const ListRequest* request, grpc::ServerReaderWriterInterface<ListReply, ListRequest>* server,
                      std::promise<grpc::Status>* status_promise);

    virtual void networks(const NetworksRequest* request,
                          grpc::ServerReaderWriterInterface<NetworksReply, NetworksRequest>* server,
                          std::promise<grpc::Status>* status_promise);

    virtual void mount(const MountRequest* request, grpc::ServerReaderWriterInterface<MountReply, MountRequest>* server,
                       std::promise<grpc::Status>* status_promise);

    virtual void recover(const RecoverRequest* request,
                         grpc::ServerReaderWriterInterface<RecoverReply, RecoverRequest>* server,
                         std::promise<grpc::Status>* status_promise);

    virtual void ssh_info(const SSHInfoRequest* request,
                          grpc::ServerReaderWriterInterface<SSHInfoReply, SSHInfoRequest>* server,
                          std::promise<grpc::Status>* status_promise);

    virtual void start(const StartRequest* request, grpc::ServerReaderWriterInterface<StartReply, StartRequest>* server,
                       std::promise<grpc::Status>* status_promise);

    virtual void stop(const StopRequest* request, grpc::ServerReaderWriterInterface<StopReply, StopRequest>* server,
                      std::promise<grpc::Status>* status_promise);

    virtual void suspend(const SuspendRequest* request,
                         grpc::ServerReaderWriterInterface<SuspendReply, SuspendRequest>* server,
                         std::promise<grpc::Status>* status_promise);

    virtual void restart(const RestartRequest* request,
                         grpc::ServerReaderWriterInterface<RestartReply, RestartRequest>* server,
                         std::promise<grpc::Status>* status_promise);

    virtual void delet(const DeleteRequest* request,
                       grpc::ServerReaderWriterInterface<DeleteReply, DeleteRequest>* server,
                       std::promise<grpc::Status>* status_promise);

    virtual void umount(const UmountRequest* request,
                        grpc::ServerReaderWriterInterface<UmountReply, UmountRequest>* server,
                        std::promise<grpc::Status>* status_promise);

    virtual void version(const VersionRequest* request,
                         grpc::ServerReaderWriterInterface<VersionReply, VersionRequest>* server,
                         std::promise<grpc::Status>* status_promise);

    virtual void get(const GetRequest* request, grpc::ServerReaderWriterInterface<GetReply, GetRequest>* server,
                     std::promise<grpc::Status>* status_promise);

    virtual void set(const SetRequest* request, grpc::ServerReaderWriterInterface<SetReply, SetRequest>* server,
                     std::promise<grpc::Status>* status_promise);

    virtual void keys(const KeysRequest* request, grpc::ServerReaderWriterInterface<KeysReply, KeysRequest>* server,
                      std::promise<grpc::Status>* status_promise);

    virtual void authenticate(const AuthenticateRequest* request,
                              grpc::ServerReaderWriterInterface<AuthenticateReply, AuthenticateRequest>* server,
                              std::promise<grpc::Status>* status_promise);
    virtual void clone(const CloneRequest* request,
                       grpc::ServerReaderWriterInterface<CloneReply, CloneRequest>* server,
                       std::promise<grpc::Status>* status_promise);

    virtual void snapshot(const SnapshotRequest* request,
                          grpc::ServerReaderWriterInterface<SnapshotReply, SnapshotRequest>* server,
                          std::promise<grpc::Status>* status_promise);

    virtual void restore(const RestoreRequest* request,
                         grpc::ServerReaderWriterInterface<RestoreReply, RestoreRequest>* server,
                         std::promise<grpc::Status>* status_promise);

private:
    void release_resources(const std::string& instance);
    void create_vm(const CreateRequest* request, grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>* server,
                   std::promise<grpc::Status>* status_promise, bool start);
    bool delete_vm(InstanceTable::iterator vm_it, bool purge, DeleteReply& response);
    grpc::Status reboot_vm(VirtualMachine& vm);
    grpc::Status shutdown_vm(VirtualMachine& vm, const std::chrono::milliseconds delay);
    grpc::Status switch_off_vm(VirtualMachine& vm);
    grpc::Status cancel_vm_shutdown(const VirtualMachine& vm);
    grpc::Status get_ssh_info_for_vm(VirtualMachine& vm, SSHInfoReply& response);

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

    MountHandler::UPtr make_mount(VirtualMachine* vm, const std::string& target, const VMMount& mount);

    struct AsyncOperationStatus
    {
        grpc::Status status;
        std::promise<grpc::Status>* status_promise;
    };

    // These async_* methods need to operate on instance names and look up the VMs again, lest they be gone or moved.
    template <typename Reply, typename Request>
    std::string async_wait_for_ssh_and_start_mounts_for(const std::string& name, const std::chrono::seconds& timeout,
                                                        grpc::ServerReaderWriterInterface<Reply, Request>* server);
    template <typename Reply, typename Request>
    AsyncOperationStatus async_wait_for_ready_all(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                                                  const std::vector<std::string>& vms,
                                                  const std::chrono::seconds& timeout,
                                                  std::promise<grpc::Status>* status_promise,
                                                  const std::string& errors,
                                                  const std::string& start_warnings);
    void finish_async_operation(const std::string& async_future_key);
    QFutureWatcher<AsyncOperationStatus>* create_future_watcher(std::function<void()> const& finished_op = []() {});
    void update_manifests_all(const bool is_force_update_from_network = false);
    // it is applied in Daemon::find wherever the image info fetching is involved, aka non-only-blueprints case
    void wait_update_manifests_all_and_optionally_applied_force(const bool force_manifest_network_download);

    template <typename Reply, typename Request>
    void reply_msg(grpc::ServerReaderWriterInterface<Reply, Request>* server, std::string&& msg, bool sticky = false);

    void
    populate_instance_info(VirtualMachine& vm, InfoReply& response, bool runtime_info, bool deleted, bool& have_mounts);

    bool is_instance_name_already_used(const std::string& instance_name);
    std::string generate_destination_instance_name_for_clone(const CloneRequest& request);
    VMSpecs clone_spec(const VMSpecs& src_vm_spec, const std::string& src_name, const std::string& dest_name);

    std::unique_ptr<const DaemonConfig> config;

protected:
    std::unordered_map<std::string, VMSpecs> vm_instance_specs;
    InstanceTable operative_instances;

    bool is_bridged(const std::string& instance_name) const;
    void add_bridged_interface(const std::string& instance_name);

private:
    InstanceTable deleted_instances;
    std::unordered_map<std::string, std::unique_ptr<DelayedShutdownTimer>> delayed_shutdown_instances;
    std::unordered_set<std::string> allocated_mac_addrs;
    DaemonRpc daemon_rpc;
    QTimer source_images_maintenance_task;
    multipass::utils::AsyncPeriodicDownloadTask<void> update_manifests_all_task{"fetch manifest periodically",
                                                                                std::chrono::minutes(15),
                                                                                std::chrono::seconds(5),
                                                                                &Daemon::update_manifests_all,
                                                                                this,
                                                                                false};
    std::unordered_map<std::string, std::unique_ptr<QFutureWatcher<AsyncOperationStatus>>> async_future_watchers;
    std::unordered_map<std::string, QFuture<std::string>> async_running_futures;
    std::mutex start_mutex;
    std::unordered_set<std::string> preparing_instances;
    QFuture<void> image_update_future;
    SettingsHandler* instance_mod_handler;
    SettingsHandler* snapshot_mod_handler;
    std::unordered_map<std::string, std::unordered_map<std::string, MountHandler::UPtr>> mounts;
    std::unordered_set<std::string> user_authorized_bridges;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_H
