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

#include "stub_mount_handler.h"
#include "stub_snapshot.h"
#include "temp_dir.h"

#include <multipass/ip_address.h>
#include <multipass/virtual_machine.h>

namespace multipass
{
namespace test
{
struct StubVirtualMachine final : public multipass::VirtualMachine
{
    StubVirtualMachine() : StubVirtualMachine{"stub"}
    {
    }

    explicit StubVirtualMachine(const std::string& name)
        : StubVirtualMachine{name, std::make_unique<TempDir>()}
    {
    }

    StubVirtualMachine(const std::string& name, std::unique_ptr<TempDir> tmp_dir)
        : VirtualMachine{name}, tmp_dir{std::move(tmp_dir)}
    {
    }

    void start() override
    {
    }

    void shutdown(ShutdownPolicy shutdown_policy = ShutdownPolicy::Powerdown) override
    {
    }

    void suspend() override
    {
    }

    multipass::VirtualMachine::State current_state() override
    {
        return multipass::VirtualMachine::State::off;
    }

    int ssh_port() override
    {
        return 42;
    }

    std::string ssh_hostname(std::chrono::milliseconds) override
    {
        return "localhost";
    }

    std::string ssh_username() override
    {
        return "ubuntu";
    }

    std::optional<IPAddress> management_ipv4() override
    {
        return std::nullopt;
    }

    std::vector<IPAddress> get_all_ipv4() override
    {
        return {IPAddress{"192.168.2.123"}};
    }

    std::string ssh_exec(const std::string& cmd, bool whisper = false) override
    {
        return {};
    }

    void wait_until_ssh_up(std::chrono::milliseconds) override
    {
    }

    void wait_for_cloud_init(std::chrono::milliseconds timeout) override
    {
    }

    void handle_state_update() override
    {
    }

    void update_cpus(int num_cores) override
    {
    }

    void resize_memory(const MemorySize&) override
    {
    }

    void resize_disk(const MemorySize&) override
    {
    }

    void add_network_interface(int, const std::string&, const NetworkInterface&) override
    {
    }

    std::unique_ptr<MountHandler> make_native_mount_handler(const std::string&,
                                                            const VMMount&) override
    {
        return std::make_unique<StubMountHandler>();
    }

    SnapshotVista view_snapshots() const override
    {
        return {};
    }

    int get_num_snapshots() const override
    {
        return 0;
    }

    std::shared_ptr<const Snapshot> get_snapshot(const std::string&) const override
    {
        return {};
    }

    std::shared_ptr<Snapshot> get_snapshot(const std::string&) override
    {
        return {};
    }

    std::shared_ptr<const Snapshot> get_snapshot(int) const override
    {
        return nullptr;
    }

    std::shared_ptr<Snapshot> get_snapshot(int) override
    {
        return nullptr;
    }

    std::shared_ptr<const Snapshot> take_snapshot(const VMSpecs&,
                                                  const std::string&,
                                                  const std::string&) override
    {
        return {};
    }

    void rename_snapshot(const std::string& old_name, const std::string& new_name) override
    {
    }

    void delete_snapshot(const std::string&) override
    {
    }

    void restore_snapshot(const std::string& name, VMSpecs& specs) override
    {
    }

    void load_snapshots() override
    {
    }

    std::vector<std::string> get_childrens_names(const Snapshot*) const override
    {
        return {};
    }

    int get_snapshot_count() const override
    {
        return 0;
    }

    QDir instance_directory() const override
    {
        return tmp_dir->path();
    }

    StubSnapshot snapshot;
    std::unique_ptr<TempDir> tmp_dir;
};
} // namespace test
} // namespace multipass
