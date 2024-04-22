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

#ifndef MULTIPASS_MOCK_SNAPSHOT_H
#define MULTIPASS_MOCK_SNAPSHOT_H

#include "common.h"

#include <multipass/memory_size.h>
#include <multipass/snapshot.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;

namespace multipass::test
{
struct MockSnapshot : public mp::Snapshot
{
    MOCK_METHOD(int, get_index, (), (const, noexcept, override));
    MOCK_METHOD(std::string, get_name, (), (const, override));
    MOCK_METHOD(std::string, get_comment, (), (const, override));
    MOCK_METHOD(std::string, get_cloud_init_instance_id, (), (const, noexcept, override));
    MOCK_METHOD(QDateTime, get_creation_timestamp, (), (const, noexcept, override));
    MOCK_METHOD(int, get_num_cores, (), (const, noexcept, override));
    MOCK_METHOD(mp::MemorySize, get_mem_size, (), (const, noexcept, override));
    MOCK_METHOD(mp::MemorySize, get_disk_space, (), (const, noexcept, override));
    MOCK_METHOD(std::vector<mp::NetworkInterface>, get_extra_interfaces, (), (const, noexcept, override));
    MOCK_METHOD(mp::VirtualMachine::State, get_state, (), (const, noexcept, override));
    MOCK_METHOD((const std::unordered_map<std::string, mp::VMMount>&), get_mounts, (), (const, noexcept, override));
    MOCK_METHOD(const QJsonObject&, get_metadata, (), (const, noexcept, override));
    MOCK_METHOD(std::shared_ptr<const Snapshot>, get_parent, (), (const, override));
    MOCK_METHOD(std::shared_ptr<Snapshot>, get_parent, (), (override));
    MOCK_METHOD(std::string, get_parents_name, (), (const, override));
    MOCK_METHOD(int, get_parents_index, (), (const, override));
    MOCK_METHOD(void, set_name, (const std::string&), (override));
    MOCK_METHOD(void, set_comment, (const std::string&), (override));
    MOCK_METHOD(void, set_parent, (std::shared_ptr<Snapshot>), (override));
    MOCK_METHOD(void, capture, (), (override));
    MOCK_METHOD(void, erase, (), (override));
    MOCK_METHOD(void, apply, (), (override));
};
} // namespace multipass::test

#endif // MULTIPASS_MOCK_SNAPSHOT_H
