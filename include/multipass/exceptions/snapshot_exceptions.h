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

#ifndef MULTIPASS_SNAPSHOT_EXCEPTIONS_H
#define MULTIPASS_SNAPSHOT_EXCEPTIONS_H

#include <stdexcept>
#include <string>

#include <multipass/format.h>

namespace multipass
{
class SnapshotNameTaken : public std::runtime_error
{
public:
    SnapshotNameTaken(const std::string& instance_name, const std::string& snapshot_name)
        : std::runtime_error{fmt::format(R"(Snapshot "{}.{}" already exists)", instance_name, snapshot_name)}
    {
    }
};

class NoSuchSnapshot : public std::runtime_error
{
public:
    NoSuchSnapshot(const std::string& vm_name, const std::string& snapshot_name)
        : std::runtime_error{fmt::format("No such snapshot: {}.{}", vm_name, snapshot_name)}
    {
    }
};
} // namespace multipass

#endif // MULTIPASS_SNAPSHOT_EXCEPTIONS_H
