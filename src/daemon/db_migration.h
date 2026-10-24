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
struct VMSpecs;

// TODO remove once no supported upgrade path predates vm-description.json
// Splits the daemon's database by writing each instance's pieces into its own
// `vm-description.json`. The daemon's database remains the source of truth for now.
class DbMigrator
{
public:
    explicit DbMigrator(const Path& data_path);

    void migrate(const std::string& name, const VMSpecs& spec, const Path& instance_dir) const;

private:
    boost::json::object vault_records;
};
} // namespace multipass
