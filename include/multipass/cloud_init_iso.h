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

#include <filesystem>
#include <string>
#include <vector>

#include <multipass/singleton.h>

#define MP_CLOUD_INIT_FILE_OPS multipass::CloudInitFileOps::instance()

namespace multipass
{
class CloudInitIso
{
public:
    void add_file(const std::string& name, const std::string& data);
    bool contains(const std::string& name) const;
    const std::string& at(const std::string& name) const;
    std::string& at(const std::string& name);
    std::string& operator[](const std::string& name);
    bool erase(const std::string& name);

    void write_to(const Path& path);
    void read_from(const std::filesystem::path& path);

    friend bool operator==(const CloudInitIso& lhs, const CloudInitIso& rhs)
    {
        return lhs.files == rhs.files;
    }

private:
    struct FileEntry
    {
        friend bool operator==(const FileEntry& lhs, const FileEntry& rhs)
        {
            return std::tie(lhs.name, lhs.data) == std::tie(rhs.name, rhs.data);
        }

        std::string name;
        std::string data;
    };
    std::vector<FileEntry> files;
};

struct NetworkInterface;
class CloudInitFileOps : public Singleton<CloudInitFileOps>
{
public:
    CloudInitFileOps(const Singleton<CloudInitFileOps>::PrivatePass&) noexcept;

    virtual void update_cloud_init_with_new_extra_interfaces_and_new_id(
        const std::string& default_mac_addr,
        const std::vector<NetworkInterface>& extra_interfaces,
        const std::string& new_instance_id,
        const std::filesystem::path& cloud_init_path) const;

    virtual void update_cloned_cloud_init(const std::string& default_mac_addr,
                                          const std::vector<NetworkInterface>& extra_interfaces,
                                          const std::string& new_hostname,
                                          const std::filesystem::path& cloud_init_path) const;
    virtual void add_extra_interface_to_cloud_init(const std::string& default_mac_addr,
                                                   const NetworkInterface& extra_interfaces,
                                                   const std::filesystem::path& cloud_init_path) const;
    virtual std::string get_instance_id_from_cloud_init(const std::filesystem::path& cloud_init_path) const;
};
} // namespace multipass
#endif // MULTIPASS_CLOUD_INIT_ISO_H
