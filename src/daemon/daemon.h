/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include <multipass/delayed_shutdown_timer.h>
#include <multipass/memory_size.h>
#include <multipass/metrics_provider.h>
#include <multipass/network_interface.h>
#include <multipass/sshfs_mount/sshfs_mounts.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_status_monitor.h>

#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QFutureWatcher>

namespace multipass
{
struct VMMount
{
    std::string source_path;
    std::unordered_map<int, int> gid_map;
    std::unordered_map<int, int> uid_map;
};

struct VMSpecs
{
    int num_cores;
    MemorySize mem_size;
    MemorySize disk_space;
    std::string default_mac_address;
    std::vector<NetworkInterface> extra_interfaces; // We want interfaces to be ordered.
    std::string ssh_username;
    VirtualMachine::State state;
    std::unordered_map<std::string, VMMount> mounts;
    bool deleted;
    QJsonObject metadata;
};

struct MetricsOptInData
{
    OptInStatus::Status opt_in_status;
    int delay_opt_in_count;
};

struct DaemonConfig;
class Daemon : public QObject, public multipass::VMStatusMonitor
{
    Q_OBJECT
public:
    explicit Daemon(std::unique_ptr<const DaemonConfig> config);
    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;

protected:
    void on_resume() override;
    void on_stop() override;
    void on_shutdown() override;
    void on_suspend() override;
    void on_restart(const std::string& name) override;
    void persist_state_for(const std::string& name, const VirtualMachine::State& state) override;
    void update_metadata_for(const std::string& name, const QJsonObject& metadata) override;
    QJsonObject retrieve_metadata_for(const std::string& name) override;

public slots:
    virtual void create(const CreateRequest* request, grpc::ServerWriter<CreateReply>* reply,
                        std::promise<grpc::Status>* status_promise);

    virtual void launch(const LaunchRequest* request, grpc::ServerWriter<LaunchReply>* reply,
                        std::promise<grpc::Status>* status_promise);

    virtual void purge(const PurgeRequest* request, grpc::ServerWriter<PurgeReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void find(const FindRequest* request, grpc::ServerWriter<FindReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void info(const InfoRequest* request, grpc::ServerWriter<InfoReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void list(const ListRequest* request, grpc::ServerWriter<ListReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void networks(const NetworksRequest* request, grpc::ServerWriter<NetworksReply>* response,
                          std::promise<grpc::Status>* status_promise);

    virtual void mount(const MountRequest* request, grpc::ServerWriter<MountReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void recover(const RecoverRequest* request, grpc::ServerWriter<RecoverReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void ssh_info(const SSHInfoRequest* request, grpc::ServerWriter<SSHInfoReply>* response,
                          std::promise<grpc::Status>* status_promise);

    virtual void start(const StartRequest* request, grpc::ServerWriter<StartReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void stop(const StopRequest* request, grpc::ServerWriter<StopReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void suspend(const SuspendRequest* request, grpc::ServerWriter<SuspendReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void restart(const RestartRequest* request, grpc::ServerWriter<RestartReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void delet(const DeleteRequest* request, grpc::ServerWriter<DeleteReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void umount(const UmountRequest* request, grpc::ServerWriter<UmountReply>* response,
                        std::promise<grpc::Status>* status_promise);

    virtual void version(const VersionRequest* request, grpc::ServerWriter<VersionReply>* response,
                         std::promise<grpc::Status>* status_promise);

private:
    void persist_instances();
    void release_resources(const std::string& instance);
    std::string check_instance_operational(const std::string& instance_name) const;
    std::string check_instance_exists(const std::string& instance_name) const;
    void create_vm(const CreateRequest* request, grpc::ServerWriter<CreateReply>* server,
                   std::promise<grpc::Status>* status_promise, bool start);
    grpc::Status reboot_vm(VirtualMachine& vm);
    grpc::Status shutdown_vm(VirtualMachine& vm, const std::chrono::milliseconds delay);
    grpc::Status cancel_vm_shutdown(const VirtualMachine& vm);
    grpc::Status cmd_vms(const std::vector<std::string>& tgts, std::function<grpc::Status(VirtualMachine&)> cmd);
    void install_sshfs(VirtualMachine* vm, const std::string& name);

    struct AsyncOperationStatus
    {
        grpc::Status status;
        std::promise<grpc::Status>* status_promise;
    };

    template <typename Reply>
    std::string async_wait_for_ssh_and_start_mounts_for(const std::string& name, grpc::ServerWriter<Reply>* server);
    template <typename Reply>
    AsyncOperationStatus async_wait_for_ready_all(grpc::ServerWriter<Reply>* server,
                                                  const std::vector<std::string>& vms,
                                                  std::promise<grpc::Status>* status_promise);
    void finish_async_operation(QFuture<AsyncOperationStatus> async_future);
    QFutureWatcher<AsyncOperationStatus>* create_future_watcher(std::function<void()> const& finished_op = []() {});

    std::unique_ptr<const DaemonConfig> config;
    std::unordered_map<std::string, VMSpecs> vm_instance_specs;
    std::unordered_map<std::string, VirtualMachine::ShPtr> vm_instances;
    std::unordered_map<std::string, VirtualMachine::ShPtr> deleted_instances;
    std::unordered_map<std::string, std::unique_ptr<DelayedShutdownTimer>> delayed_shutdown_instances;
    std::unordered_set<std::string> allocated_mac_addrs;
    DaemonRpc daemon_rpc;
    QTimer source_images_maintenance_task;
    MetricsProvider metrics_provider;
    MetricsOptInData metrics_opt_in;
    SSHFSMounts instance_mounts;
    std::vector<std::unique_ptr<QFutureWatcher<AsyncOperationStatus>>> async_future_watchers;
    std::unordered_map<std::string, QFuture<std::string>> async_running_futures;
    std::mutex start_mutex;
    std::unordered_set<std::string> preparing_instances;
    QFuture<void> image_update_future;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_H
