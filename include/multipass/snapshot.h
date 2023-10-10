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

#include <memory>
#include <string>
#include <unordered_map>

class QJsonObject;
class QDateTime;

namespace multipass
{
class MemorySize;
struct VMMount;

class Snapshot : private DisabledCopyMove
{
public: // TODO@snapshots drop any accessors that we end up not needing
    virtual ~Snapshot() = default;

    virtual int get_index() const = 0;
    virtual std::string get_name() const = 0;
    virtual std::string get_comment() const = 0;
    virtual QDateTime get_creation_timestamp() const = 0;
    virtual int get_num_cores() const noexcept = 0;
    virtual MemorySize get_mem_size() const noexcept = 0;
    virtual MemorySize get_disk_space() const noexcept = 0;
    virtual VirtualMachine::State get_state() const noexcept = 0;

    // Note that these return references - careful not to delete the snapshot while they are in use
    virtual const std::unordered_map<std::string, VMMount>& get_mounts() const noexcept = 0;
    virtual const QJsonObject& get_metadata() const noexcept = 0;

    virtual std::shared_ptr<const Snapshot> get_parent() const = 0;
    virtual std::shared_ptr<Snapshot> get_parent() = 0;
    virtual std::string get_parents_name() const = 0;
    virtual int get_parents_index() const = 0; // TODO@no-merge use this to reference parents in json and head_file

    virtual QJsonObject serialize() const = 0; // TODO@no-merge remove
    virtual void persist() const = 0; // TODO@no-merge can we avoid exposing this at all (just persist when pertinent)?

    virtual void set_name(const std::string&) = 0; // TODO@snapshots don't forget to rename json file
    virtual void set_comment(const std::string&) = 0;
    virtual void set_parent(std::shared_ptr<Snapshot>) = 0;

    virtual void capture() = 0; // not using the constructor, we need snapshot objects for existing snapshots too
    virtual void erase() = 0;   // not using the destructor, we want snapshots to stick around when daemon quits
    virtual void apply() = 0;
};
} // namespace multipass

#endif // MULTIPASS_SNAPSHOT_H
