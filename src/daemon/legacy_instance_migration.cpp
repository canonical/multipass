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

#include "legacy_instance_migration.h"
#include "default_vm_image_vault.h"

#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/network_interface.h>
#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_mount.h>

#include <QDir>

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "daemon";
constexpr auto image_db_name = "vault/multipassd-instance-image-records.json";

constexpr std::array db_keys{"num_cores",
                             "mem_size",
                             "disk_space",
                             "ssh_username",
                             "zone",
                             "mac_addr",
                             "extra_interfaces",
                             "metadata",
                             "state",
                             "clone_count",
                             "deleted",
                             "mounts"};

boost::json::object load_image_records(const mp::Path& data_path)
{
    const auto path = QDir{data_path}.filePath(image_db_name);
    try
    {
        if (const auto data = MP_FILEOPS.try_read_file(
                std::filesystem::path{path.toStdU16String()});
            data && !data->empty())
            return boost::json::parse(*data).as_object();
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "Could not read the image records in {}: {}", path, e.what());
    }
    return {};
}

// keys already moved by earlier versions get defaults here, which never overwrite the file
void fill_from_db_record(mp::VirtualMachineDescription& desc,
                         const boost::json::value& record,
                         const std::string& default_zone)
{
    const auto mem_size = mp::lookup_or<std::string>(record, "mem_size", "");
    const auto disk_space = mp::lookup_or<std::string>(record, "disk_space", "");
    const auto ssh_username = mp::lookup_or<std::string>(record, "ssh_username", "");

    desc.num_cores = mp::lookup_or<int>(record, "num_cores", 0);
    desc.mem_size = mp::MemorySize{mem_size.empty() ? mp::default_memory_size : mem_size};
    desc.disk_space = mp::MemorySize{disk_space.empty() ? mp::default_disk_size : disk_space};
    desc.ssh_username = ssh_username.empty() ? "ubuntu" : ssh_username;
    desc.zone = mp::lookup_or<std::string>(record, "zone", default_zone);
    desc.default_mac_address = mp::lookup_or<std::string>(record, "mac_addr", "");
    desc.extra_interfaces = mp::lookup_or<std::vector<mp::NetworkInterface>>(record,
                                                                             "extra_interfaces",
                                                                             {});
    desc.metadata = mp::lookup_or<boost::json::object>(record, "metadata", {});
    desc.state = static_cast<mp::VirtualMachine::State>(mp::lookup_or<int>(record, "state", 0));
    desc.clone_count = mp::lookup_or<int>(record, "clone_count", 0);
    desc.deleted = mp::lookup_or<bool>(record, "deleted", false);
    desc.mounts = mp::lookup_or<std::unordered_map<std::string, mp::VMMount>>(
        record,
        "mounts",
        {},
        mp::MapAsJsonArray{"target_path"});
}

mp::VMImage image_from(const boost::json::value& record)
{
    auto vault_record = value_to<mp::VaultRecord>(record);
    // instances launched from aliases before 1.16 have no OS in their records
    if (vault_record.query.query_type == mp::Query::Type::Alias && vault_record.image.os.empty())
        vault_record.image.os = "Ubuntu";
    return vault_record.image;
}

void add_missing_keys(const std::string& name,
                      const mp::VirtualMachineDescription& legacy_desc,
                      const std::vector<const char*>& keys,
                      const mp::Path& instance_dir)
{
    const auto path = QDir{instance_dir}.filePath(mp::vm_description_file_name);
    try
    {
        boost::json::object stored;
        if (const auto data = MP_FILEOPS.try_read_file(
                std::filesystem::path{path.toStdU16String()}))
            stored = boost::json::parse(*data).as_object();

        const auto legacy_fields = boost::json::value_from(legacy_desc);
        auto changed = false;
        for (const auto* key : keys)
            changed |= stored.emplace(key, legacy_fields.as_object().at(key)).second;

        if (!changed)
            return;

        MP_FILEOPS.write_transactionally(path, mp::pretty_print(stored));
    }
    catch (const std::exception& e)
    {
        throw std::runtime_error{
            fmt::format("Could not migrate the description of '{}': {}", name, e.what())};
    }
    mpl::info(category, "Migrated the description of {} to {}", name, path);
}
} // namespace

mp::LegacyInstanceMigrator::LegacyInstanceMigrator(const Path& data_path, std::string default_zone)
    : default_zone{std::move(default_zone)}, image_records{load_image_records(data_path)}
{
}

bool mp::LegacyInstanceMigrator::is_ghost(const boost::json::value& db_record) const
{
    // Ghost records predate vm-description.json, so only records with the legacy keys qualify
    return db_record.as_object().contains("num_cores") &&
           !value_to<int>(db_record.at("num_cores")) &&
           !lookup_or<bool>(db_record, "deleted", false) &&
           lookup_or<std::string>(db_record, "ssh_username", "").empty() &&
           lookup_or<boost::json::object>(db_record, "metadata", {}).empty() &&
           !MemorySize{lookup_or<std::string>(db_record, "mem_size", "")}.in_bytes() &&
           !MemorySize{lookup_or<std::string>(db_record, "disk_space", "")}.in_bytes();
}

void mp::LegacyInstanceMigrator::migrate(const std::string& name,
                                         const boost::json::value& db_record,
                                         const Path& instance_dir) const
{
    VirtualMachineDescription legacy_desc{};
    legacy_desc.vm_name = name;
    legacy_desc.cloud_init_iso = MP_PLATFORM.qstr_to_path(instance_dir) / cloud_init_file_name;
    std::vector<const char*> keys{"vm_name", "cloud_init_iso"};

    const auto& record = db_record.as_object();
    if (std::ranges::any_of(db_keys, [&record](const auto* key) { return record.contains(key); }))
    {
        fill_from_db_record(legacy_desc, db_record, default_zone);
        keys.insert(keys.end(), db_keys.begin(), db_keys.end());
    }

    if (const auto* image_record = image_records.if_contains(name))
    {
        legacy_desc.image = image_from(*image_record);
        keys.push_back("image");
    }

    add_missing_keys(name, legacy_desc, keys, instance_dir);
}
