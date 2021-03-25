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

#include <multipass/default_vm_workflow_provider.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>

#include <QFile>
#include <QFileInfo>

#include <Poco/StreamCopier.h>
#include <Poco/Zip/ZipArchive.h>
#include <Poco/Zip/ZipStream.h>

#include <fstream>
#include <sstream>

namespace mp = multipass;

const QString github_workflows_archive_name{"multipass-workflows.zip"};
const QString workflow_dir_version{"v1"};
constexpr int len = 65536u;

mp::DefaultVMWorkflowProvider::DefaultVMWorkflowProvider(const QUrl& workflows_url, URLDownloader* downloader,
                                                         VMImageVault* image_vault, const QDir& archive_dir)
    : workflows_url{workflows_url},
      url_downloader{downloader},
      image_vault{image_vault},
      archive_file_path{archive_dir.filePath(github_workflows_archive_name)}
{
    fetch_workflows_archive();
    get_workflow_map();
}

mp::VMImageInfo mp::DefaultVMWorkflowProvider::fetch_workflow(const Query& /* query */)
{
    return {};
}

void mp::DefaultVMWorkflowProvider::for_each_entry_do(const Action& /* action */)
{
}

std::vector<mp::VMImageInfo> mp::DefaultVMWorkflowProvider::all_workflows()
{
    return {};
}

void mp::DefaultVMWorkflowProvider::fetch_workflows_archive()
{
    url_downloader->download_to(workflows_url, archive_file_path, -1, -1, [](auto...) { return true; });
}

void mp::DefaultVMWorkflowProvider::get_workflow_map()
{
    std::ifstream zip_stream{archive_file_path.toStdString()};
    Poco::Zip::ZipArchive zip_archive{zip_stream};

    for (auto it = zip_archive.headerBegin(); it != zip_archive.headerEnd(); ++it)
    {
        if (it->second.isFile())
        {
            auto file_name = it->second.getFileName();
            QFileInfo file_info{QString::fromStdString(file_name)};

            if (file_info.path().contains(workflow_dir_version) && file_info.suffix() == "yaml")
            {

                Poco::Zip::ZipInputStream zip_input_stream{zip_stream, it->second};
                std::ostringstream out(std::ios::binary);
                Poco::StreamCopier::copyStream(zip_input_stream, out);
                workflow_map[file_info.baseName().toStdString()] = out.str();
            }
        }
    }
}
