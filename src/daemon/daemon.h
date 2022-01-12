/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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
#include <multipass/id_mappings.h>
#include <multipass/memory_size.h>
#include <multipass/metrics_provider.h>
#include <multipass/network_interface.h>
#include <multipass/sshfs_mount/sshfs_mounts.h>
#include <multipass/virtual_machine.h>
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
struct VMMount
{
    std::string source_path;
    id_mappings gid_mappings;
    id_mappings uid_mappings;
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

    void persist_instances();

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
    virtual void create(const CreateRequest* request, grpc::ServerWriterInterface<CreateReply>* reply,
                        std::promise<grpc::Status>* status_promise);

    virtual void launch(const LaunchRequest* request, grpc::ServerWriterInterface<LaunchReply>* reply,
                        std::promise<grpc::Status>* status_promise);

    virtual void purge(const PurgeRequest* request, grpc::ServerWriterInterface<PurgeReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void find(const FindRequest* request, grpc::ServerWriterInterface<FindReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void info(const InfoRequest* request, grpc::ServerWriterInterface<InfoReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void list(const ListRequest* request, grpc::ServerWriterInterface<ListReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void networks(const NetworksRequest* request, grpc::ServerWriterInterface<NetworksReply>* response,
                          std::promise<grpc::Status>* status_promise);

    virtual void mount(const MountRequest* request, grpc::ServerWriterInterface<MountReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void recover(const RecoverRequest* request, grpc::ServerWriterInterface<RecoverReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void ssh_info(const SSHInfoRequest* request, grpc::ServerWriterInterface<SSHInfoReply>* response,
                          std::promise<grpc::Status>* status_promise);

    virtual void start(const StartRequest* request, grpc::ServerWriterInterface<StartReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void stop(const StopRequest* request, grpc::ServerWriterInterface<StopReply>* response,
                      std::promise<grpc::Status>* status_promise);

    virtual void suspend(const SuspendRequest* request, grpc::ServerWriterInterface<SuspendReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void restart(const RestartRequest* request, grpc::ServerWriterInterface<RestartReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void delet(const DeleteRequest* request, grpc::ServerWriterInterface<DeleteReply>* response,
                       std::promise<grpc::Status>* status_promise);

    virtual void umount(const UmountRequest* request, grpc::ServerWriterInterface<UmountReply>* response,
                        std::promise<grpc::Status>* status_promise);

    virtual void version(const VersionRequest* request, grpc::ServerWriterInterface<VersionReply>* response,
                         std::promise<grpc::Status>* status_promise);

    virtual void get(const GetRequest* request, grpc::ServerWriterInterface<GetReply>* response,
                     std::promise<grpc::Status>* status_promise);

    virtual void authenticate(const AuthenticateRequest* request,
                              grpc::ServerWriterInterface<AuthenticateReply>* response,
                              std::promise<grpc::Status>* status_promise);

private:
    void release_resources(const std::string& instance);
    std::string check_instance_operational(const std::string& instance_name) const;
    std::string check_instance_exists(const std::string& instance_name) const;
    void create_vm(const CreateRequest* request, grpc::ServerWriterInterface<CreateReply>* server,
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
    std::string async_wait_for_ssh_and_start_mounts_for(const std::string& name, const std::chrono::seconds& timeout,
                                                        grpc::ServerWriterInterface<Reply>* server);
    template <typename Reply>
    AsyncOperationStatus
    async_wait_for_ready_all(grpc::ServerWriterInterface<Reply>* server, const std::vector<std::string>& vms,
                             const std::chrono::seconds& timeout, std::promise<grpc::Status>* status_promise);
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
