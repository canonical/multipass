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
#include <multipass/constants.h>
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
const auto snapshot_template =
    QStringLiteral("@s%1"); /* avoid confusion with snapshot names by prepending a character
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
        throw std::runtime_error{
            fmt::format("Could not open snapshot file for for reading: {}", file.fileName())};

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

std::unordered_map<std::string, mp::VMMount> load_mounts(const QJsonArray& mounts_json)
{
    std::unordered_map<std::string, mp::VMMount> mounts;
    for (const auto& entry : mounts_json)
    {
        const auto json = mp::qjson_to_boost_json(entry);
        mounts[entry.toObject()["target_path"].toString().toStdString()] =
            value_to<mp::VMMount>(json);
    }

    return mounts;
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

// When it does not contain cloud_init_instance_id, it signifies that the legacy snapshot does not
// have the item and it needs to fill cloud_init_instance_id with the current value. The current
// value equals to the value at snapshot time because cloud_init_instance_id has been an immutable
// variable up to this point.
std::string choose_cloud_init_instance_id(const QJsonObject& json,
                                          const std::filesystem::path& cloud_init_iso_path)
{
    return json.contains("cloud_init_instance_id")
               ? json["cloud_init_instance_id"].toString().toStdString()
               : MP_CLOUD_INIT_FILE_OPS.get_instance_id_from_cloud_init(cloud_init_iso_path);
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
}

mp::BaseSnapshot::BaseSnapshot(SnapshotDescription desc_, VirtualMachine& vm_, bool captured_)
    : desc{std::move(desc_)},
      id{snapshot_template.arg(desc.index)},
      storage_dir{vm_.instance_directory()},
      captured{captured_}
{
    parent = find_parent(desc, vm_);
    desc.parent_index = parent ? parent->get_index() : 0;
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
    : BaseSnapshot{read_snapshot_json(filename), vm, desc}
{
}

mp::BaseSnapshot::BaseSnapshot(const QJsonObject& json,
                               VirtualMachine& vm,
                               const VirtualMachineDescription& desc)
    : BaseSnapshot{
          {json["name"].toString().toStdString(),
           json["comment"].toString().toStdString(),
           json["parent"].toInt(),
           choose_cloud_init_instance_id(
               json,
               std::filesystem::path{vm.instance_directory().absolutePath().toStdString()} /
                   cloud_init_file_name),
           json["index"].toInt(),
           QDateTime::fromString(json["creation_timestamp"].toString(), Qt::ISODateWithMs),
           json["num_cores"].toInt(),
           MemorySize{json["mem_size"].toString().toStdString()},
           MemorySize{json["disk_space"].toString().toStdString()},
           MP_JSONUTILS.read_extra_interfaces(json).value_or(desc.extra_interfaces),
           static_cast<mp::VirtualMachine::State>(json["state"].toInt()),
           load_mounts(json["mounts"].toArray()),
           qjson_to_boost_json(json["metadata"]).as_object()},
          vm,
          /*captured=*/true}
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

    snapshot.insert("name", QString::fromStdString(desc.name));
    snapshot.insert("comment", QString::fromStdString(desc.comment));
    snapshot.insert("cloud_init_instance_id", QString::fromStdString(desc.cloud_init_instance_id));
    snapshot.insert("parent", get_parents_index());
    snapshot.insert("index", desc.index);
    snapshot.insert("creation_timestamp", desc.creation_timestamp.toString(Qt::ISODateWithMs));
    snapshot.insert("num_cores", desc.num_cores);
    snapshot.insert("mem_size", QString::number(desc.mem_size.in_bytes()));
    snapshot.insert("disk_space", QString::number(desc.disk_space.in_bytes()));
    snapshot.insert("extra_interfaces",
                    MP_JSONUTILS.extra_interfaces_to_json_array(desc.extra_interfaces));
    snapshot.insert("state", static_cast<int>(desc.state));
    snapshot.insert("metadata", boost_json_to_qjson(desc.metadata));

    // Extract mount serialization
    QJsonArray json_mounts;
    for (const auto& mount : desc.mounts)
    {
        auto entry = mp::boost_json_to_qjson(boost::json::value_from(mount.second)).toObject();
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
    MP_FILEOPS.write_transactionally(snapshot_filepath, QJsonDocument{serialize()}.toJson());
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
