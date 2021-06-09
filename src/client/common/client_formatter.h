/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_CLIENT_FORMATTER_H
#define MULTIPASS_CLIENT_FORMATTER_H

#include <multipass/cli/alias_dict.h>

#include <map>

namespace multipass
{
class ClientFormatter
{
public:
    ClientFormatter() : preferred_format("table")
    {
    }
    ClientFormatter(const std::string& format) : preferred_format(format)
    {
    }

    std::string format(const AliasDict& aliases) const;

private:
    typedef std::map<AliasDict::key_type, AliasDict::mapped_type> sorted_map;

    sorted_map sort_dict(const AliasDict& aliases) const;
    std::string format_csv(const AliasDict& aliases) const;
    std::string format_json(const AliasDict& aliases) const;
    std::string format_table(const AliasDict& aliases) const;
    std::string format_yaml(const AliasDict& aliases) const;

    std::string preferred_format;
};
} // namespace multipass
#endif // MULTIPASS_CLIENT_FORMATTER_H
