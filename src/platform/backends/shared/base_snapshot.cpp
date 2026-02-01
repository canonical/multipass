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

#include "base_snapshot.h"
#include "multipass/virtual_machine.h"

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_mount.h>
#include <multipass/vm_specs.h>
#include <scope_guard.hpp>

#include <QString>

#include <QFile>
#include <QTemporaryDir>
#include <stdexcept>

namespace mp = multipass;

namespace
{
constexpr auto snapshot_extension = "snapshot.json";
constexpr auto index_digits = 4; // these two go together
const auto snapshot_template =
    QStringLiteral("@s%1"); /* avoid confusion with snapshot names by prepending a character
                               that can't be part of the name (users can call a snapshot
                               "s1", but they cannot call it "@s1") */

QString derive_index_string(int index)
{
    return QString{"%1"}.arg(index, index_digits, 10, QLatin1Char('0'));
}

mp::SnapshotDescription read_snapshot_json(const QString& filename,
                                           const mp::VirtualMachine& vm,
                                           const mp::VirtualMachineDescription& vm_desc)
{
    QFile file{filename};
    if (!MP_FILEOPS.open(file, QIODevice::ReadOnly))
        throw std::runtime_error{
            fmt::format("Could not open snapshot file for for reading: {}", file.fileName())};

    const auto& data = MP_FILEOPS.read_all(file);
    if (data.isEmpty())
        throw std::runtime_error{fmt::format("Empty snapshot JSON: {}", file.fileName())};

    try
    {
        const auto json = boost::json::parse(std::string_view(data));
        return value_to<mp::SnapshotDescription>(json.at("snapshot"),
                                                 mp::snapshot_context{vm, vm_desc});
    }
    catch (const boost::system::system_error& e)
    {
        throw std::runtime_error{fmt::format("Could not parse snapshot JSON; error: {}; file: {}",
                                             e.what(),
                                             file.fileName())};
    }
}

std::shared_ptr<mp::Snapshot> find_parent(const mp::SnapshotDescription& desc,
                                          mp::VirtualMachine& vm)
{
    try
    {
        return desc.parent_index ? vm.get_snapshot(desc.parent_index) : nullptr;
    }
    catch (std::out_of_range&)
    {
        throw std::runtime_error{
            fmt::format("Missing snapshot parent. Snapshot name: {}; parent index: {}",
                        desc.name,
                        desc.parent_index)};
    }
}
} // namespace

mp::BaseSnapshot::BaseSnapshot(SnapshotDescription desc_,
                               std::shared_ptr<Snapshot> parent_,
                               VirtualMachine& vm_,
                               bool captured_)
    : desc{std::move(desc_)},
      parent{std::move(parent_)},
      id{snapshot_template.arg(desc.index)},
      storage_dir{vm_.instance_directory()},
      captured{captured_}
{
    desc.parent_index = parent ? parent->get_index() : 0;
    if (captured && this->desc.upgraded)
        persist();
}

mp::BaseSnapshot::BaseSnapshot(SnapshotDescription desc_, VirtualMachine& vm_, bool captured_)
    : desc{std::move(desc_)},
      id{snapshot_template.arg(desc.index)},
      storage_dir{vm_.instance_directory()},
      captured{captured_}
{
    parent = find_parent(desc, vm_);
    desc.parent_index = parent ? parent->get_index() : 0;
    if (captured && desc.upgraded)
        persist();
}

mp::BaseSnapshot::BaseSnapshot(const std::string& name,
                               const std::string& comment,
                               const std::string& cloud_init_instance_id,
                               std::shared_ptr<Snapshot> parent,
                               const VMSpecs& specs,
                               VirtualMachine& vm)
    : BaseSnapshot{{name,
                    comment,
                    parent ? parent->get_index() : 0,
                    cloud_init_instance_id,
                    vm.get_snapshot_count() + 1,
                    QDateTime::currentDateTimeUtc(),
                    specs.num_cores,
                    specs.mem_size,
                    specs.disk_space,
                    specs.extra_interfaces,
                    specs.state,
                    specs.mounts,
                    specs.metadata},
                   std::move(parent),
                   vm,
                   /*captured=*/false}
{
}

mp::BaseSnapshot::BaseSnapshot(const QString& filename,
                               VirtualMachine& vm,
                               const VirtualMachineDescription& desc)
    : BaseSnapshot{read_snapshot_json(filename, vm, desc), vm, /*captured=*/true}
{
}

void mp::BaseSnapshot::persist() const
{
    assert(captured && "precondition: only captured snapshots can be persisted");
    const std::unique_lock lock{mutex};

    auto snapshot_filepath = storage_dir.filePath(derive_snapshot_filename());
    boost::json::value json = {{"snapshot", boost::json::value_from(desc)}};
    MP_FILEOPS.write_transactionally(snapshot_filepath, pretty_print(json));
}

auto mp::BaseSnapshot::erase_helper()
{
    // Remove snapshot file
    auto tmp_dir = std::make_unique<QTemporaryDir>(); // work around no move ctor
    if (!tmp_dir->isValid())
        throw std::runtime_error{"Could not create temporary directory"};

    const auto snapshot_filename = derive_snapshot_filename();
    auto snapshot_filepath = storage_dir.filePath(snapshot_filename);
    auto deleting_filepath = tmp_dir->filePath(snapshot_filename);

    QFile snapshot_file{snapshot_filepath};
    if (!MP_FILEOPS.rename(snapshot_file, deleting_filepath) && MP_FILEOPS.exists(snapshot_file))
        throw std::runtime_error{
            fmt::format("Failed to move snapshot file to temporary destination: {}",
                        deleting_filepath)};

    return sg::make_scope_guard([tmp_dir = std::move(tmp_dir),
                                 deleting_filepath = std::move(deleting_filepath),
                                 snapshot_filepath = std::move(snapshot_filepath)]() noexcept {
        QFile temp_file{deleting_filepath};
        MP_FILEOPS.rename(temp_file, snapshot_filepath); // best effort, ignore return
    });
}

void mp::BaseSnapshot::erase()
{
    const std::unique_lock lock{mutex};
    assert(captured && "precondition: only captured snapshots can be erased");

    auto rollback_snapshot_file = erase_helper();
    erase_impl();
    rollback_snapshot_file.dismiss();
}

QString mp::BaseSnapshot::derive_snapshot_filename() const
{
    return QString{"%1.%2"}.arg(derive_index_string(desc.index), snapshot_extension);
}
