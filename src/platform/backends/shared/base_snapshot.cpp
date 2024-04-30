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

#include <multipass/cloud_init_iso.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_mount.h>
#include <multipass/vm_specs.h>

#include <scope_guard.hpp>

#include <QJsonArray>
#include <QString>

#include <QFile>
#include <QJsonParseError>
#include <QTemporaryDir>
#include <stdexcept>

namespace mp = multipass;

namespace
{
constexpr auto snapshot_extension = "snapshot.json";
constexpr auto index_digits = 4; // these two go together
constexpr auto max_snapshots = 9999;
const auto snapshot_template = QStringLiteral("@s%1"); /* avoid confusion with snapshot names by prepending a character
                                                          that can't be part of the name (users can call a snapshot
                                                          "s1", but they cannot call it "@s1") */

QString derive_index_string(int index)
{
    return QString{"%1"}.arg(index, index_digits, 10, QLatin1Char('0'));
}

QJsonObject read_snapshot_json(const QString& filename)
{
    QFile file{filename};
    if (!MP_FILEOPS.open(file, QIODevice::ReadOnly))
        throw std::runtime_error{fmt::format("Could not open snapshot file for for reading: {}", file.fileName())};

    QJsonParseError parse_error{};
    const auto& data = MP_FILEOPS.read_all(file);

    if (const auto json = QJsonDocument::fromJson(data, &parse_error).object(); parse_error.error)
        throw std::runtime_error{fmt::format("Could not parse snapshot JSON; error: {}; file: {}",
                                             parse_error.errorString(),
                                             file.fileName())};
    else if (json.isEmpty())
        throw std::runtime_error{fmt::format("Empty snapshot JSON: {}", file.fileName())};
    else
        return json["snapshot"].toObject();
}

QJsonObject read_snapshot_json_and_update_unique_identifiers(const QString& filename,
                                                             const multipass::VMSpecs& src_specs,
                                                             const multipass::VMSpecs& dest_specs,
                                                             const std::string& src_vm_name,
                                                             const std::string& dest_vm_name)
{
    QJsonObject snapshot_json = read_snapshot_json(filename);
    snapshot_json["cloud_init_instance_id"] =
        MP_JSONUTILS.update_cloud_init_instance_id(snapshot_json["cloud_init_instance_id"], src_vm_name, dest_vm_name);
    snapshot_json["metadata"] = MP_JSONUTILS.update_unique_identifiers_of_metadata(snapshot_json["metadata"],
                                                                                   src_specs,
                                                                                   dest_specs,
                                                                                   src_vm_name,
                                                                                   dest_vm_name);
    return snapshot_json;
}

std::unordered_map<std::string, mp::VMMount> load_mounts(const QJsonArray& mounts_json)
{
    std::unordered_map<std::string, mp::VMMount> mounts;
    for (const auto& entry : mounts_json)
    {
        const auto& json = entry.toObject();
        mounts[json["target_path"].toString().toStdString()] = mp::VMMount{json};
    }

    return mounts;
}

std::shared_ptr<mp::Snapshot> find_parent(const QJsonObject& json, mp::VirtualMachine& vm)
{
    auto parent_idx = json["parent"].toInt();
    try
    {
        return parent_idx ? vm.get_snapshot(parent_idx) : nullptr;
    }
    catch (std::out_of_range&)
    {
        throw std::runtime_error{fmt::format("Missing snapshot parent. Snapshot name: {}; parent index: {}",
                                             json["name"].toString(),
                                             parent_idx)};
    }
}

// When it does not contain cloud_init_instance_id, it signifies that the legacy snapshot does not have the
// item and it needs to fill cloud_init_instance_id with the current value. The current value equals to the
// value at snapshot time because cloud_init_instance_id has been an immutable variable up to this point.
std::string choose_cloud_init_instance_id(const QJsonObject& json, const std::filesystem::path& cloud_init_iso_path)
{
    return json.contains("cloud_init_instance_id")
               ? json["cloud_init_instance_id"].toString().toStdString()
               : MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(cloud_init_iso_path);
}
} // namespace

mp::BaseSnapshot::BaseSnapshot(const std::string& name,    // NOLINT(modernize-pass-by-value)
                               const std::string& comment, // NOLINT(modernize-pass-by-value)
                               const std::string& cloud_init_instance_id,
                               std::shared_ptr<Snapshot> parent,
                               int index,
                               QDateTime&& creation_timestamp,
                               int num_cores,
                               MemorySize mem_size,
                               MemorySize disk_space,
                               std::vector<NetworkInterface> extra_interfaces,
                               VirtualMachine::State state,
                               std::unordered_map<std::string, VMMount> mounts,
                               QJsonObject metadata,
                               const QDir& storage_dir,
                               bool captured)
    : name{name},
      comment{comment},
      parent{std::move(parent)},
      cloud_init_instance_id{cloud_init_instance_id},
      index{index},
      id{snapshot_template.arg(index)},
      creation_timestamp{std::move(creation_timestamp)},
      num_cores{num_cores},
      mem_size{mem_size},
      disk_space{disk_space},
      extra_interfaces{extra_interfaces},
      state{state},
      mounts{std::move(mounts)},
      metadata{std::move(metadata)},
      storage_dir{storage_dir},
      captured{captured}
{
    using St = VirtualMachine::State;
    if (state != St::off && state != St::stopped)
        throw std::runtime_error{fmt::format("Unsupported VM state in snapshot: {}", static_cast<int>(state))};
    if (index < 1)
        throw std::runtime_error{fmt::format("Snapshot index not positive: {}", index)};
    if (index > max_snapshots)
        throw std::runtime_error{fmt::format("Maximum number of snapshots exceeded: {}", index)};
    if (name.empty())
        throw std::runtime_error{"Snapshot names cannot be empty"};
    if (num_cores < 1)
        throw std::runtime_error{fmt::format("Invalid number of cores for snapshot: {}", num_cores)};
    if (auto mem_bytes = mem_size.in_bytes(); mem_bytes < 1)
        throw std::runtime_error{fmt::format("Invalid memory size for snapshot: {}", mem_bytes)};
    if (auto disk_bytes = disk_space.in_bytes(); disk_bytes < 1)
        throw std::runtime_error{fmt::format("Invalid disk size for snapshot: {}", disk_bytes)};
}

mp::BaseSnapshot::BaseSnapshot(const std::string& name,
                               const std::string& comment,
                               const std::string& cloud_init_instance_id,
                               std::shared_ptr<Snapshot> parent,
                               const VMSpecs& specs,
                               const VirtualMachine& vm)
    : BaseSnapshot{name,
                   comment,
                   cloud_init_instance_id,
                   std::move(parent),
                   vm.get_snapshot_count() + 1,
                   QDateTime::currentDateTimeUtc(),
                   specs.num_cores,
                   specs.mem_size,
                   specs.disk_space,
                   specs.extra_interfaces,
                   specs.state,
                   specs.mounts,
                   specs.metadata,
                   vm.instance_directory(),
                   /*captured=*/false}
{
    assert(index > 0 && "snapshot indices need to start at 1");
}

mp::BaseSnapshot::BaseSnapshot(const QString& filename,
                               const VMSpecs& src_specs,
                               const VMSpecs& dest_specs,
                               const std::string& src_vm_name,
                               VirtualMachine& dest_vm,
                               const VirtualMachineDescription& desc)
    : BaseSnapshot{read_snapshot_json_and_update_unique_identifiers(filename,
                                                                    src_specs,
                                                                    dest_specs,
                                                                    src_vm_name,
                                                                    dest_vm.get_vm_name()),
                   dest_vm,
                   desc}
{
    persist();
}

mp::BaseSnapshot::BaseSnapshot(const QString& filename, VirtualMachine& vm, const VirtualMachineDescription& desc)
    : BaseSnapshot{read_snapshot_json(filename), vm, desc}
{
}

mp::BaseSnapshot::BaseSnapshot(const QJsonObject& json, VirtualMachine& vm, const VirtualMachineDescription& desc)
    : BaseSnapshot{
          json["name"].toString().toStdString(),    // name
          json["comment"].toString().toStdString(), // comment
          choose_cloud_init_instance_id(json,
                                        std::filesystem::path{vm.instance_directory().absolutePath().toStdString()} /
                                            "cloud-init-config.iso"), // instance id from cloud init
          find_parent(json, vm),                                      // parent
          json["index"].toInt(),                                      // index
          QDateTime::fromString(json["creation_timestamp"].toString(), Qt::ISODateWithMs), // creation_timestamp
          json["num_cores"].toInt(),                                                       // num_cores
          MemorySize{json["mem_size"].toString().toStdString()},                           // mem_size
          MemorySize{json["disk_space"].toString().toStdString()},                         // disk_space
          MP_JSONUTILS.read_extra_interfaces(json).value_or(desc.extra_interfaces),        // extra_interfaces
          static_cast<mp::VirtualMachine::State>(json["state"].toInt()),                   // state
          load_mounts(json["mounts"].toArray()),                                           // mounts
          json["metadata"].toObject(),                                                     // metadata
          vm.instance_directory(),                                                         // storage_dir
          true}                                                                            // captured
{
    if (!(json.contains("extra_interfaces") && json.contains("cloud_init_instance_id")))
    {
        persist();
    }
}

QJsonObject mp::BaseSnapshot::serialize() const
{
    assert(captured && "precondition: only captured snapshots can be serialized");
    QJsonObject ret, snapshot{};
    const std::unique_lock lock{mutex};

    snapshot.insert("name", QString::fromStdString(name));
    snapshot.insert("comment", QString::fromStdString(comment));
    snapshot.insert("cloud_init_instance_id", QString::fromStdString(cloud_init_instance_id));
    snapshot.insert("parent", get_parents_index());
    snapshot.insert("index", index);
    snapshot.insert("creation_timestamp", creation_timestamp.toString(Qt::ISODateWithMs));
    snapshot.insert("num_cores", num_cores);
    snapshot.insert("mem_size", QString::number(mem_size.in_bytes()));
    snapshot.insert("disk_space", QString::number(disk_space.in_bytes()));
    snapshot.insert("extra_interfaces", MP_JSONUTILS.extra_interfaces_to_json_array(extra_interfaces));
    snapshot.insert("state", static_cast<int>(state));
    snapshot.insert("metadata", metadata);

    // Extract mount serialization
    QJsonArray json_mounts;
    for (const auto& mount : mounts)
    {
        auto entry = mount.second.serialize();
        entry.insert("target_path", QString::fromStdString(mount.first));
        json_mounts.append(entry);
    }

    snapshot.insert("mounts", json_mounts);
    ret.insert("snapshot", snapshot);

    return ret;
}

void mp::BaseSnapshot::persist() const
{
    const std::unique_lock lock{mutex};

    auto snapshot_filepath = storage_dir.filePath(derive_snapshot_filename());
    MP_JSONUTILS.write_json(serialize(), snapshot_filepath);
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
    if (!MP_FILEOPS.rename(snapshot_file, deleting_filepath) && snapshot_file.exists())
        throw std::runtime_error{
            fmt::format("Failed to move snapshot file to temporary destination: {}", deleting_filepath)};

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
    return QString{"%1.%2"}.arg(derive_index_string(index), snapshot_extension);
}
