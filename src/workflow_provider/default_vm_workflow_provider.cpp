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
#include <multipass/exceptions/workflow_minimum_exception.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <yaml-cpp/yaml.h>

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

namespace
{
auto workflows_map_for(const std::string& archive_file_path)
{
    std::map<std::string, std::string> workflows_map;
    std::ifstream zip_stream{archive_file_path, std::ios::binary};
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
                workflows_map[file_info.baseName().toStdString()] = out.str();
            }
        }
    }

    return workflows_map;
}
} // namespace

mp::DefaultVMWorkflowProvider::DefaultVMWorkflowProvider(const QUrl& workflows_url, URLDownloader* downloader,
                                                         const QDir& archive_dir)
    : workflows_url{workflows_url},
      url_downloader{downloader},
      archive_file_path{archive_dir.filePath(github_workflows_archive_name)}
{
    fetch_workflows();
}

mp::DefaultVMWorkflowProvider::DefaultVMWorkflowProvider(URLDownloader* downloader, const QDir& archive_dir)
    : DefaultVMWorkflowProvider(default_workflow_url, downloader, archive_dir)
{
}

mp::Query mp::DefaultVMWorkflowProvider::fetch_workflow_for(const std::string& workflow_name,
                                                            VirtualMachineDescription& vm_desc)
{
    Query query{"", "", false, "", Query::Type::Alias};
    auto& config = workflow_map.at(workflow_name);
    auto workflow_config = YAML::Load(config);

    auto workflow_instance = workflow_config["instances"][workflow_name];

    if (workflow_instance["image"])
    {
        // TODO: Support http later.
        // This only supports the "alias" and "remote:alias" scheme at this time
        auto image_str{workflow_config["instances"][workflow_name]["image"].as<std::string>()};
        auto tokens = mp::utils::split(image_str, ":");

        if (tokens.size() == 2)
        {
            query.remote_name = tokens[0];
            query.release = tokens[1];
        }
        else if (tokens.size() == 1)
        {
            query.release = tokens[0];
        }
        else
        {
            throw std::runtime_error("Unsupported image scheme in Workflow");
        }
    }

    if (workflow_instance["limits"]["min-cpu"])
    {
        auto min_cpus = workflow_instance["limits"]["min-cpu"].as<int>();

        if (vm_desc.num_cores == 0)
        {
            vm_desc.num_cores = min_cpus;
        }
        else if (vm_desc.num_cores < min_cpus)
        {
            throw WorkflowMinimumException("Number of CPUs", std::to_string(min_cpus));
        }
    }

    if (workflow_instance["limits"]["min-mem"])
    {
        auto min_mem_size_str{workflow_instance["limits"]["min-mem"].as<std::string>()};
        MemorySize min_mem_size{min_mem_size_str};

        if (vm_desc.mem_size.in_bytes() == 0)
        {
            vm_desc.mem_size = min_mem_size;
        }
        else if (vm_desc.mem_size < min_mem_size)
        {
            throw WorkflowMinimumException("Memory size", min_mem_size_str);
        }
    }

    if (workflow_instance["limits"]["min-disk"])
    {
        auto min_disk_space_str{workflow_instance["limits"]["min-disk"].as<std::string>()};
        MemorySize min_disk_space{min_disk_space_str};

        if (vm_desc.disk_space.in_bytes() == 0)
        {
            vm_desc.disk_space = min_disk_space;
        }
        else if (vm_desc.disk_space < min_disk_space)
        {
            throw WorkflowMinimumException("Disk space", min_disk_space_str);
        }
    }

    if (workflow_instance["cloud-init"])
    {
        for (const auto& node : workflow_instance["cloud-init"])
        {
            if (node.first.IsScalar())
            {
                vm_desc.vendor_data_config[node.first.Scalar()] = node.second;
            }
        }
    }

    return query;
}

mp::VMImageInfo mp::DefaultVMWorkflowProvider::info_for(const std::string& workflow_name)
{
    auto& config = workflow_map.at(workflow_name);
    auto workflow_config = YAML::Load(config);

    VMImageInfo image_info;
    image_info.aliases.append(QString::fromStdString(workflow_name));
    image_info.release_title = QString::fromStdString(workflow_config["description"].as<std::string>());

    return image_info;
}

std::vector<mp::VMImageInfo> mp::DefaultVMWorkflowProvider::all_workflows()
{
    std::vector<VMImageInfo> workflow_info;

    for (const auto& [key, config] : workflow_map)
    {
        auto workflow_config = YAML::Load(config);

        VMImageInfo image_info;
        image_info.aliases.append(QString::fromStdString(key));
        image_info.release_title = QString::fromStdString(workflow_config["description"].as<std::string>());

        workflow_info.push_back(image_info);
    }

    return workflow_info;
}

void mp::DefaultVMWorkflowProvider::fetch_workflows()
{
    url_downloader->download_to(workflows_url, archive_file_path, -1, -1, [](auto...) { return true; });

    workflow_map = workflows_map_for(archive_file_path.toStdString());
}
