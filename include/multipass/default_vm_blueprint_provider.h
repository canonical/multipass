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

#ifndef MULTIPASS_DEFAULT_VM_BLUEPRINT_PROVIDER_H
#define MULTIPASS_DEFAULT_VM_BLUEPRINT_PROVIDER_H

#include <multipass/path.h>
#include <multipass/vm_blueprint_provider.h>

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QString>
#include <QSysInfo>
#include <QUrl>

#include <chrono>
#include <map>

namespace multipass
{
const QString default_blueprint_url{"https://codeload.github.com/canonical/multipass-blueprints/zip/refs/heads/main"};

class URLDownloader;

class DefaultVMBlueprintProvider final : public VMBlueprintProvider
{
public:
    DefaultVMBlueprintProvider(const QUrl& blueprints_url, URLDownloader* downloader, const QDir& cache_dir_path,
                               const std::chrono::milliseconds& blueprints_ttl,
                               const QString& arch = QSysInfo::currentCpuArchitecture());
    DefaultVMBlueprintProvider(URLDownloader* downloader, const QDir& cache_dir_path,
                               const std::chrono::milliseconds& blueprints_ttl,
                               const QString& arch = QSysInfo::currentCpuArchitecture());

    Query fetch_blueprint_for(const std::string& blueprint_name, VirtualMachineDescription& vm_desc,
                              ClientLaunchData& client_launch_data) override;
    Query blueprint_from_file(const std::string& path, const std::string& blueprint_name,
                              VirtualMachineDescription& vm_desc, ClientLaunchData& client_launch_data) override;
    std::optional<VMImageInfo> info_for(const std::string& blueprint_name) override;
    std::vector<VMImageInfo> all_blueprints() override;
    std::string name_from_blueprint(const std::string& blueprint_name) override;
    int blueprint_timeout(const std::string& blueprint_name) override;

private:
    void fetch_blueprints();
    void update_blueprints();

    const QUrl blueprints_url;
    URLDownloader* const url_downloader;
    const QString archive_file_path;
    const std::chrono::milliseconds blueprints_ttl;
    std::chrono::steady_clock::time_point last_update;
    std::map<std::string, YAML::Node> blueprint_map;
    bool needs_update{true};
    const QString arch;
};
} // namespace multipass
#endif // MULTIPASS_DEFAULT_VM_BLUEPRINT_PROVIDER_H
