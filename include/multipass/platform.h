/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
#include <multipass/process.h>
#include <multipass/sshfs_server_config.h>
#include <multipass/update_prompt.h>
#include <multipass/virtual_machine_factory.h>

#include <libssh/sftp.h>

#include <QString>

#include <memory>
#include <string>
#include <vector>

namespace multipass
{
namespace platform
{
std::map<QString, QString> extra_settings_defaults();

QString interpret_winterm_integration(const QString& val);
void sync_winterm_profiles();

QString autostart_test_data(); // returns a platform-specific string, for testing purposes
void setup_gui_autostart_prerequisites();

std::string default_server_address();
QString default_driver();

QString daemon_config_home();                      // temporary

bool is_backend_supported(const QString& backend); // temporary
VirtualMachineFactory::UPtr vm_backend(const Path& data_dir);

logging::Logger::UPtr make_logger(logging::Level level);
UpdatePrompt::UPtr make_update_prompt();
std::unique_ptr<Process> make_sshfs_server_process(const SSHFSServerConfig& config);
int chown(const char* path, unsigned int uid, unsigned int gid);
bool symlink(const char* target, const char* link, bool is_dir);
bool link(const char* target, const char* link);
int utime(const char* path, int atime, int mtime);
int symlink_attr_from(const char* path, sftp_attributes_struct* attr);
bool is_alias_supported(const std::string& alias, const std::string& remote);
bool is_remote_supported(const std::string& remote);
bool is_image_url_supported();

int wait_for_quit_signals();
} // namespace platform
} // namespace multipass
#endif // MULTIPASS_PLATFORM_H
