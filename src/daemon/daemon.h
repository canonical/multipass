/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
#include <multipass/sshfs_mount/sshfs_mount.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_status_monitor.h>

#include <future>
#include <memory>
#include <unordered_map>

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
    std::string mac_addr;
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
    void persist_state_for(const std::string& name) override;
    void update_metadata_for(const std::string& name, const QJsonObject& metadata) override;
    QJsonObject retrieve_metadata_for(const std::string& name) override;

public slots:
    grpc::Status create(grpc::ServerContext* context, const CreateRequest* request,
                        grpc::ServerWriter<CreateReply>* reply) override;

    void launch(const LaunchRequest* request, grpc::ServerWriter<LaunchReply>* reply,
                std::promise<grpc::Status>* status_promise);

    void purge(const PurgeRequest* request, grpc::ServerWriter<PurgeReply>* response,
               std::promise<grpc::Status>* status_promise);

    void find(const FindRequest* request, grpc::ServerWriter<FindReply>* response,
              std::promise<grpc::Status>* status_promise);

    void info(const InfoRequest* request, grpc::ServerWriter<InfoReply>* response,
              std::promise<grpc::Status>* status_promise);

    void list(const ListRequest* request, grpc::ServerWriter<ListReply>* response,
              std::promise<grpc::Status>* status_promise);

    void mount(const MountRequest* request, grpc::ServerWriter<MountReply>* response,
               std::promise<grpc::Status>* status_promise);

    void recover(const RecoverRequest* request, grpc::ServerWriter<RecoverReply>* response,
                 std::promise<grpc::Status>* status_promise);

    void ssh_info(const SSHInfoRequest* request, grpc::ServerWriter<SSHInfoReply>* response,
                  std::promise<grpc::Status>* status_promise);

    void start(const StartRequest* request, grpc::ServerWriter<StartReply>* response,
               std::promise<grpc::Status>* status_promise);

    void stop(const StopRequest* request, grpc::ServerWriter<StopReply>* response,
              std::promise<grpc::Status>* status_promise);

    void suspend(const SuspendRequest* request, grpc::ServerWriter<SuspendReply>* response,
                 std::promise<grpc::Status>* status_promise);

    void restart(const RestartRequest* request, grpc::ServerWriter<RestartReply>* response,
                 std::promise<grpc::Status>* status_promise);

    void delet(const DeleteRequest* request, grpc::ServerWriter<DeleteReply>* response,
               std::promise<grpc::Status>* status_promise);

    void umount(const UmountRequest* request, grpc::ServerWriter<UmountReply>* response,
                std::promise<grpc::Status>* status_promise);

    void version(const VersionRequest* request, grpc::ServerWriter<VersionReply>* response,
                 std::promise<grpc::Status>* status_promise);

private:
    void persist_instances();
    void start_mount(const VirtualMachine::UPtr& vm, const std::string& name, const std::string& source_path,
                     const std::string& target_path, const std::unordered_map<int, int>& gid_map,
                     const std::unordered_map<int, int>& uid_map);
    void stop_mounts_for_instance(const std::string& instance);
    void release_resources(const std::string& instance);
    std::string check_instance_operational(const std::string& instance_name) const;
    std::string check_instance_exists(const std::string& instance_name) const;
    grpc::Status create_vm(grpc::ServerContext* context, const CreateRequest* request,
                           grpc::ServerWriter<CreateReply>* server, bool start);
    grpc::Status reboot_vm(VirtualMachine& vm);
    grpc::Status shutdown_vm(VirtualMachine& vm, const std::chrono::milliseconds delay);
    grpc::Status cancel_vm_shutdown(const VirtualMachine& vm);
    grpc::Status cmd_vms(const std::vector<std::string>& tgts, std::function<grpc::Status(VirtualMachine&)> cmd);
    void install_sshfs(const VirtualMachine::UPtr& vm, const std::string& name);

    std::unique_ptr<const DaemonConfig> config;
    std::unordered_map<std::string, VMSpecs> vm_instance_specs;
    std::unordered_map<std::string, VirtualMachine::UPtr> vm_instances;
    std::unordered_map<std::string, VirtualMachine::UPtr> deleted_instances;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<SshfsMount>>> mount_threads;
    std::unordered_map<std::string, std::unique_ptr<DelayedShutdownTimer>> delayed_shutdown_instances;
    std::unordered_set<std::string> allocated_mac_addrs;
    std::unordered_map<std::string, VMImageHost*> remote_image_host_map;
    DaemonRpc daemon_rpc;
    QTimer source_images_maintenance_task;
    MetricsProvider metrics_provider;
    MetricsOptInData metrics_opt_in;
};
} // namespace multipass
#endif // MULTIPASS_DAEMON_H
