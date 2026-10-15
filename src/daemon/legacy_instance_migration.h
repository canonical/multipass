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

#pragma once

#include <multipass/path.h>

#include <boost/json.hpp>

#include <string>

namespace multipass
{
// TODO remove once no supported upgrade path predates vm-description.json
class LegacyInstanceMigrator
{
public:
    LegacyInstanceMigrator(const Path& data_path, std::string default_zone);

    [[nodiscard]] bool is_ghost(const boost::json::value& db_record) const;

    // Fills in what the instance's vm-description.json lacks from the instance db and vault records
    void migrate(const std::string& name,
                 const boost::json::value& db_record,
                 const Path& instance_dir) const;

private:
    std::string default_zone;
    boost::json::object image_records;
};
} // namespace multipass
