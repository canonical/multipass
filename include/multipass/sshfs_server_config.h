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

#ifndef MULTIPASS_SSHFS_SERVER_CONFIG_H
#define MULTIPASS_SSHFS_SERVER_CONFIG_H

#include <multipass/id_mappings.h>

#include <string>
#include <unordered_map>

namespace multipass
{

struct SSHFSServerConfig
{
    std::string host;
    int port;
    std::string username;
    std::string instance;
    std::string private_key;
    std::string source_path;
    std::string target_path;
    id_mappings gid_mappings;
    id_mappings uid_mappings;
};

} // namespace multipass
#endif // MULTIPASS_SSHFS_SERVER_CONFIG_H
