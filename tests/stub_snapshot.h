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

#ifndef MULTIPASS_STUB_SNAPSHOT_H
#define MULTIPASS_STUB_SNAPSHOT_H

#include <multipass/memory_size.h>
#include <multipass/snapshot.h>

#include <QJsonObject>

namespace multipass::test
{
struct StubSnapshot : public Snapshot
{
    std::string get_name() const noexcept override
    {
        return {};
    }

    std::string get_comment() const noexcept override
    {
        return {};
    }

    std::string get_cloud_init_instance_id() const noexcept override
    {
        return {};
    }

    QDateTime get_creation_timestamp() const noexcept override
    {
        return QDateTime{};
    }

    std::string get_parents_name() const override
    {
        return {};
    }

    std::shared_ptr<const Snapshot> get_parent() const noexcept override
    {
        return nullptr;
    }

    std::shared_ptr<Snapshot> get_parent() override
    {
        return nullptr;
    }

    int get_index() const noexcept override
    {
        return 0;
    }

    int get_parents_index() const override
    {
        return 0;
    }

    int get_num_cores() const noexcept override
    {
        return 0;
    }

    MemorySize get_mem_size() const noexcept override
    {
        return MemorySize{};
    }

    MemorySize get_disk_space() const noexcept override
    {
        return MemorySize{};
    }

    std::vector<NetworkInterface> get_extra_interfaces() const noexcept override
    {
        return std::vector<NetworkInterface>{};
    }

    VirtualMachine::State get_state() const noexcept override
    {
        return VirtualMachine::State::off;
    }

    const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept override
    {
        return mounts;
    }

    const QJsonObject& get_metadata() const noexcept override
    {
        return metadata;
    }

    void set_name(const std::string&) override
    {
    }

    void set_comment(const std::string&) override
    {
    }

    void set_parent(std::shared_ptr<Snapshot>) override
    {
    }

    void capture() override
    {
    }

    void erase() override
    {
    }

    void apply() override
    {
    }

    std::unordered_map<std::string, VMMount> mounts;
    QJsonObject metadata;
};
} // namespace multipass::test

#endif // MULTIPASS_STUB_SNAPSHOT_H
