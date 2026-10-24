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

#include "default_vm_image_vault.h"
#include "db_migration.h"

#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/json_utils.h>
#include <multipass/logging/log.h>
#include <multipass/platform.h>
#include <multipass/query.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>

#include <stdexcept>
#include <tuple>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "daemon";

boost::json::object load_vault_instance_records(const mp::Path& data_path)
{
    const auto path = MP_PLATFORM.qstr_to_path(data_path) / "vault" /
                      "multipassd-instance-image-records.json";
    try
    {
        if (const auto data = MP_FILEOPS.try_read_file(path); data && !data->empty())
            return boost::json::parse(*data).as_object();
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "Could not read the image records in {}: {}", path.string(), e.what());
    }

    return {};
}
} // namespace

mp::DbMigrator::DbMigrator(const Path& data_path)
    : vault_records{load_vault_instance_records(data_path)}
{
}

void mp::DbMigrator::migrate(const std::string& name,
                             const VMSpecs& spec,
                             const Path& instance_dir) const
{
    const auto instance_path = MP_PLATFORM.qstr_to_path(instance_dir);
    const auto path = instance_path / vm_description_file_name;
    const auto* vault_record_json = vault_records.if_contains(name);
    try
    {
        auto description = boost::json::value_from(spec).as_object();
        description["vm_name"] = name;
        description["schema_version"] = vm_description_schema_version;
        description["cloud_init_iso"] =
            MP_PLATFORM.path_to_qstr(instance_path / cloud_init_file_name).toStdString();

        if (vault_record_json)
        {
            auto vault_record = value_to<VaultRecord>(*vault_record_json);
            // instances launched from aliases before 1.16 have no OS in their records
            if (vault_record.query.query_type == Query::Type::Alias &&
                vault_record.image.os.empty())
                vault_record.image.os = "Ubuntu";
            description["image"] = boost::json::value_from(vault_record.image);
        }

        MP_FILEOPS.write_transactionally(path, pretty_print(description));
    }
    catch (const std::exception& e)
    {
        mpl::warn(category, "Could not migrate the instance record of {}: {}", name, e.what());
        return;
    }
}
