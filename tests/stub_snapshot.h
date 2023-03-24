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
    const std::string& get_name() const noexcept override
    {
        return name;
    }

    const std::string& get_comment() const noexcept override
    {
        return comment;
    }

    const Snapshot* get_parent() const noexcept override
    {
        return nullptr;
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

    std::string name{};
    std::string comment{};
    std::unordered_map<std::string, VMMount> mounts;
    QJsonObject metadata;
};
} // namespace multipass::test

#endif // MULTIPASS_STUB_SNAPSHOT_H
