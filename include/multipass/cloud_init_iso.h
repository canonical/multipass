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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_CLOUD_INIT_ISO_H
#define MULTIPASS_CLOUD_INIT_ISO_H

#include <multipass/path.h>

#include <string>
#include <vector>

namespace multipass
{
class CloudInitIso
{
public:
    void add_file(const std::string& name, const std::string& data);
    void write_to(const Path& path);

private:
    struct FileEntry
    {
        std::string name;
        std::string data;
    };
    std::vector<FileEntry> files;
};
}
#endif // MULTIPASS_CLOUD_INIT_ISO_H
