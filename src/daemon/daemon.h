/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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
#include "vm_specs.h"

#include <multipass/delayed_shutdown_timer.h>
#include <multipass/mount_handler.h>
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
    void on_resume() override;
    void on_stop() override;
    void on_shutdown() override;
    void on_suspend() override;
    void on_restart(const std::string& name) override;
    void persist_state_for(const std::string& name, const VirtualMachine::State& state) override;
    void update_metadata_for(const std::string& name, const QJsonObject& metadata) override;
    QJsonObject retrieve_metadata_for(const std::string& name) override;

public slots:
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

private:
    void release_resources(const std::string& instance);
    std::string check_instance_operational(const std::string& instance_name) const;
    std::string check_instance_exists(const std::string& instance_name) const;
    void create_vm(const CreateRequest* request, grpc::ServerReaderWriterInterface<CreateReply, CreateRequest>* server,
                   std::promise<grpc::Status>* status_promise, bool start);
    grpc::Status reboot_vm(VirtualMachine& vm);
    grpc::Status shutdown_vm(VirtualMachine& vm, const std::chrono::milliseconds delay);
    grpc::Status cancel_vm_shutdown(const VirtualMachine& vm);
    grpc::Status cmd_vms(const std::vector<std::string>& tgts, std::function<grpc::Status(VirtualMachine&)> cmd);
    void init_mounts(const std::string& name);

    struct AsyncOperationStatus
    {
        grpc::Status status;
        std::promise<grpc::Status>* status_promise;
    };

    template <typename Reply, typename Request>
    std::string async_wait_for_ssh_and_start_mounts_for(const std::string& name, const std::chrono::seconds& timeout,
                                                        grpc::ServerReaderWriterInterface<Reply, Request>* server);
    template <typename Reply, typename Request>
    AsyncOperationStatus
    async_wait_for_ready_all(grpc::ServerReaderWriterInterface<Reply, Request>* server,
                             const std::vector<std::string>& vms, const std::chrono::seconds& timeout,
                             std::promise<grpc::Status>* status_promise, const std::string& errors);
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
    std::vector<std::unique_ptr<QFutureWatcher<AsyncOperationStatus>>> async_future_watchers;
    std::unordered_map<std::string, QFuture<std::string>> async_running_futures;
    std::mutex start_mutex;
    std::unordered_set<std::string> preparing_instances;
    QFuture<void> image_update_future;
    SettingsHandler* instance_mod_handler;
    std::unordered_map<std::string, std::unordered_map<std::string, MountHandler::UPtr>> mounts;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_H
