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

#ifndef MULTIPASS_QUERY_H
#define MULTIPASS_QUERY_H

#include <string>

namespace multipass
{
class Query
{
public:
    enum Type
    {
        Alias,
        LocalFile,
        HttpDownload
    };

    std::string name;
    std::string release;
    bool persistent;
    std::string remote_name;
    Type query_type;
    bool allow_unsupported{false};
};
}
#endif // MULTIPASS_QUERY_H
