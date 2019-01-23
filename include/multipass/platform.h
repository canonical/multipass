/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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

#ifndef MULTIPASS_PLATFORM_H
#define MULTIPASS_PLATFORM_H

#include <multipass/logging/logger.h>
#include <multipass/sshfs_server_config.h>
#include <multipass/virtual_machine_factory.h>

#include <libssh/sftp.h>
#include <QProcess>

#include <memory>
#include <string>

namespace multipass
{
namespace platform
{
std::string default_server_address();
VirtualMachineFactory::UPtr vm_backend(const Path& data_dir);
std::unique_ptr<QProcess> make_sshfs_server_process(const SSHFSServerConfig& config);
logging::Logger::UPtr make_logger(logging::Level level);
int chown(const char* path, unsigned int uid, unsigned int gid);
bool symlink(const char* target, const char* link, bool is_dir);
bool link(const char* target, const char* link);
int utime(const char* path, int atime, int mtime);
int symlink_attr_from(const char* path, sftp_attributes_struct* attr);
bool is_alias_supported(const std::string& alias, const std::string& remote);
bool is_remote_supported(const std::string& remote);
bool is_image_url_supported();
} // namespace platform
} // namespace multipass
#endif // MULTIPASS_PLATFORM_H
