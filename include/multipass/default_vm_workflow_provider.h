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

#ifndef MULTIPASS_DEFAULT_VM_WORKFLOW_PROVIDER_H
#define MULTIPASS_DEFAULT_VM_WORKFLOW_PROVIDER_H

#include <multipass/path.h>
#include <multipass/vm_workflow_provider.h>

#include <QDir>
#include <QString>
#include <QUrl>

#include <map>

namespace multipass
{
class URLDownloader;
class VMImageVault;

class DefaultVMWorkflowProvider final : public VMWorkflowProvider
{
public:
    DefaultVMWorkflowProvider(const QUrl& workflows_url, URLDownloader* downloader, VMImageVault* image_vault,
                              const QDir& cache_dir_path);

    VMImageInfo fetch_workflow(const Query& query) override;
    void for_each_entry_do(const Action& action) override;
    std::vector<VMImageInfo> all_workflows() override;

private:
    void fetch_workflows_archive();
    void get_workflow_map();

    const QUrl workflows_url;
    URLDownloader* const url_downloader;
    VMImageVault* const image_vault;
    const QString archive_file_path;
    std::map<std::string, std::string> workflow_map;
};
} // namespace multipass
#endif // MULTIPASS_DEFAULT_VM_WORKFLOW_PROVIDER_H
