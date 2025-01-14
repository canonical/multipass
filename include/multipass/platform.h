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

#ifndef MULTIPASS_PLATFORM_H
#define MULTIPASS_PLATFORM_H

#include <multipass/alias_definition.h>
#include <multipass/days.h>
#include <multipass/logging/logger.h>
#include <multipass/network_interface_info.h>
#include <multipass/process/process.h>
#include <multipass/process/process_spec.h>
#include <multipass/settings/setting_spec.h>
#include <multipass/singleton.h>
#include <multipass/sshfs_server_config.h>
#include <multipass/update_prompt.h>
#include <multipass/virtual_machine_factory.h>
#include <multipass/vm_image_vault.h>

#include <QDir>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#define MP_PLATFORM multipass::platform::Platform::instance()

struct sftp_attributes_struct;

namespace multipass
{
namespace platform
{
class Platform : public Singleton<Platform>
{
public:
    Platform(const Singleton::PrivatePass&) noexcept;
    // Get information on the network interfaces that are seen by the platform, indexed by name
    virtual std::map<std::string, NetworkInterfaceInfo> get_network_interfaces_info() const;
    virtual QString get_blueprints_url_override() const;
    virtual bool is_alias_supported(const std::string& alias, const std::string& remote) const;
    virtual bool is_remote_supported(const std::string& remote) const;
    virtual bool is_backend_supported(const QString& backend) const; // temporary (?)
    virtual int chown(const char* path, unsigned int uid, unsigned int gid) const;
    virtual int chmod(const char* path, unsigned int mode) const;
    virtual bool set_permissions(const multipass::Path path, const QFileDevice::Permissions permissions) const;
    virtual bool link(const char* target, const char* link) const;
    virtual bool symlink(const char* target, const char* link, bool is_dir) const;
    virtual int utime(const char* path, int atime, int mtime) const;
    virtual QString get_username() const;
    virtual QDir get_alias_scripts_folder() const;
    virtual void create_alias_script(const std::string& alias, const AliasDefinition& def) const;
    virtual void remove_alias_script(const std::string& alias) const;
    virtual std::string alias_path_message() const;
    virtual void set_server_socket_restrictions(const std::string& server_address, const bool restricted) const;
    virtual QString multipass_storage_location() const;
    virtual QString daemon_config_home() const; // temporary
    virtual SettingSpec::Set extra_daemon_settings() const;
    virtual SettingSpec::Set extra_client_settings() const;
    virtual QString default_driver() const;
    virtual QString default_privileged_mounts() const;
    virtual bool is_image_url_supported() const;
    [[nodiscard]] virtual std::string bridge_nomenclature() const;
    virtual int get_cpus() const;
    virtual long long get_total_ram() const;
};

QString interpret_setting(const QString& key, const QString& val);
void sync_winterm_profiles();

std::string default_server_address();

VirtualMachineFactory::UPtr vm_backend(const Path& data_dir);
logging::Logger::UPtr make_logger(logging::Level level);
UpdatePrompt::UPtr make_update_prompt();
std::unique_ptr<Process> make_sshfs_server_process(const SSHFSServerConfig& config);
std::unique_ptr<Process> make_process(std::unique_ptr<ProcessSpec>&& process_spec);
int symlink_attr_from(const char* path, sftp_attributes_struct* attr);

std::function<int()> make_quit_watchdog(); // call while single-threaded; call result later, in dedicated thread

std::string reinterpret_interface_id(const std::string& ux_id); // give platforms a chance to reinterpret network IDs

std::string host_version();

} // namespace platform
} // namespace multipass

inline multipass::platform::Platform::Platform(const PrivatePass& pass) noexcept : Singleton(pass)
{
}

#endif // MULTIPASS_PLATFORM_H
