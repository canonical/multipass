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

#ifndef MULTIPASS_SNAPSHOT_H
#define MULTIPASS_SNAPSHOT_H

#include "disabled_copy_move.h"
#include "virtual_machine.h"

#include <string>
#include <unordered_map>

class QJsonObject;

namespace multipass
{
class MemorySize;

class Snapshot : private DisabledCopyMove
{
public:
    virtual ~Snapshot() = default;

    virtual const std::string& get_name() const noexcept = 0;
    virtual const std::string& get_comment() const noexcept = 0;
    virtual const Snapshot* get_parent() const noexcept = 0;

    virtual int get_num_cores() const noexcept = 0;
    virtual MemorySize get_mem_size() const noexcept = 0;
    virtual MemorySize get_disk_space() const noexcept = 0;
    virtual VirtualMachine::State get_state() const noexcept = 0;
    virtual const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept = 0;
    virtual const QJsonObject& get_metadata() const noexcept = 0;
};
} // namespace multipass

#endif // MULTIPASS_SNAPSHOT_H
