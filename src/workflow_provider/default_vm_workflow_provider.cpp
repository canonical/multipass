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
#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/exceptions/workflow_exceptions.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/poco_zip_utils.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>

#include <QFile>
#include <QFileInfo>

#include <Poco/StreamCopier.h>
#include <Poco/Zip/ZipStream.h>

#include <fstream>
#include <sstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const QString github_workflows_archive_name{"multipass-workflows.zip"};
const QString workflow_dir_version{"v1"};
constexpr auto category = "workflow provider";

auto workflows_map_for(const std::string& archive_file_path, bool& needs_update)
{
    std::map<std::string, YAML::Node> workflows_map;
    std::ifstream zip_stream{archive_file_path, std::ios::binary};
    auto zip_archive = MP_POCOZIPUTILS.zip_archive_for(zip_stream);

    for (auto it = zip_archive.headerBegin(); it != zip_archive.headerEnd(); ++it)
    {
        if (it->second.isFile())
        {
            auto file_name = it->second.getFileName();
            QFileInfo file_info{QString::fromStdString(file_name)};

            if (file_info.dir().dirName() == workflow_dir_version &&
                (file_info.suffix() == "yaml" || file_info.suffix() == "yml"))
            {
                if (!mp::utils::valid_hostname(file_info.baseName().toStdString()))
                {
                    mpl::log(
                        mpl::Level::error, category,
                        fmt::format("Invalid workflow name \'{}\': must be a valid host name", file_info.baseName()));
                    needs_update = true;

                    continue;
                }

                Poco::Zip::ZipInputStream zip_input_stream{zip_stream, it->second};
                std::ostringstream out(std::ios::binary);
                Poco::StreamCopier::copyStream(zip_input_stream, out);
                workflows_map[file_info.baseName().toStdString()] = YAML::Load(out.str());
            }
        }
    }

    return workflows_map;
}
} // namespace

mp::DefaultVMWorkflowProvider::DefaultVMWorkflowProvider(const QUrl& workflows_url, URLDownloader* downloader,
                                                         const QDir& archive_dir,
                                                         const std::chrono::milliseconds& workflows_ttl)
    : workflows_url{workflows_url},
      url_downloader{downloader},
      archive_file_path{archive_dir.filePath(github_workflows_archive_name)},
      workflows_ttl{workflows_ttl}
{
    try
    {
        update_workflows();
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::error, category, fmt::format("Error on workflows start up: {}", e.what()));
    }
}

mp::DefaultVMWorkflowProvider::DefaultVMWorkflowProvider(URLDownloader* downloader, const QDir& archive_dir,
                                                         const std::chrono::milliseconds& workflows_ttl)
    : DefaultVMWorkflowProvider(default_workflow_url, downloader, archive_dir, workflows_ttl)
{
}

mp::Query mp::DefaultVMWorkflowProvider::fetch_workflow_for(const std::string& workflow_name,
                                                            VirtualMachineDescription& vm_desc)
{
    update_workflows();

    Query query{"", "", false, "", Query::Type::Alias};
    auto& workflow_config = workflow_map.at(workflow_name);

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
            needs_update = true;
            throw InvalidWorkflowException("Unsupported image scheme in Workflow");
        }
    }

    if (workflow_instance["limits"]["min-cpu"])
    {
        try
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
        catch (const YAML::BadConversion&)
        {
            needs_update = true;
            throw InvalidWorkflowException(fmt::format("Minimum CPU value in workflow is invalid"));
        }
    }

    if (workflow_instance["limits"]["min-mem"])
    {
        auto min_mem_size_str{workflow_instance["limits"]["min-mem"].as<std::string>()};

        try
        {
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
        catch (const InvalidMemorySizeException&)
        {
            needs_update = true;
            throw InvalidWorkflowException(fmt::format("Minimum memory size value in workflow is invalid"));
        }
    }

    if (workflow_instance["limits"]["min-disk"])
    {
        auto min_disk_space_str{workflow_instance["limits"]["min-disk"].as<std::string>()};

        try
        {
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
        catch (const InvalidMemorySizeException&)
        {
            needs_update = true;
            throw InvalidWorkflowException(fmt::format("Minimum disk space value in workflow is invalid"));
        }
    }

    if (workflow_instance["cloud-init"]["vendor-data"])
    {
        try
        {
            vm_desc.vendor_data_config = YAML::Load(workflow_instance["cloud-init"]["vendor-data"].as<std::string>());
        }
        catch (const YAML::BadConversion&)
        {
            needs_update = true;
            throw InvalidWorkflowException(
                fmt::format("Cannot convert cloud-init data for the {} workflow", workflow_name));
        }
    }

    return query;
}

mp::VMImageInfo mp::DefaultVMWorkflowProvider::info_for(const std::string& workflow_name)
{
    update_workflows();

    static constexpr auto missing_key_template{"The \'{}\' key is required for the {} workflow"};
    static constexpr auto bad_conversion_template{"Cannot convert \'{}\' key for the {} workflow"};
    auto& workflow_config = workflow_map.at(workflow_name);

    VMImageInfo image_info;
    image_info.aliases.append(QString::fromStdString(workflow_name));

    const auto description_key{"description"};
    const auto version_key{"version"};

    if (!workflow_config[description_key])
    {
        needs_update = true;
        throw InvalidWorkflowException(fmt::format(missing_key_template, description_key, workflow_name));
    }

    if (!workflow_config[version_key])
    {
        needs_update = true;
        throw InvalidWorkflowException(fmt::format(missing_key_template, version_key, workflow_name));
    }

    try
    {
        image_info.release_title = QString::fromStdString(workflow_config[description_key].as<std::string>());
    }
    catch (const YAML::BadConversion&)
    {
        needs_update = true;
        throw InvalidWorkflowException(fmt::format(bad_conversion_template, description_key, workflow_name));
    }

    try
    {
        image_info.version = QString::fromStdString(workflow_config["version"].as<std::string>());
    }
    catch (const YAML::BadConversion&)
    {
        needs_update = true;
        throw InvalidWorkflowException(fmt::format(bad_conversion_template, version_key, workflow_name));
    }

    return image_info;
}

std::vector<mp::VMImageInfo> mp::DefaultVMWorkflowProvider::all_workflows()
{
    update_workflows();

    bool will_need_update{false};
    std::vector<VMImageInfo> workflow_info;

    for (const auto& [key, config] : workflow_map)
    {
        try
        {
            workflow_info.push_back(info_for(key));
        }
        catch (const InvalidWorkflowException& e)
        {
            // Don't force updates in info_for() since we are looping and only force the update once we
            // finish iterating.
            needs_update = false;
            will_need_update = true;
            mpl::log(mpl::Level::error, category, fmt::format("Invalid workflow: {}", e.what()));
        }
    }

    if (will_need_update)
        needs_update = true;

    return workflow_info;
}

void mp::DefaultVMWorkflowProvider::fetch_workflows()
{
    url_downloader->download_to(workflows_url, archive_file_path, -1, -1, [](auto...) { return true; });

    workflow_map = workflows_map_for(archive_file_path.toStdString(), needs_update);
}

void mp::DefaultVMWorkflowProvider::update_workflows()
{
    const auto now = std::chrono::steady_clock::now();
    if ((now - last_update) > workflows_ttl || needs_update)
    {
        try
        {
            fetch_workflows();
            last_update = now;
            needs_update = false;
        }
        catch (const Poco::Exception& e)
        {
            mpl::log(mpl::Level::error, category,
                     fmt::format("Error extracting Workflows zip file: {}", e.displayText()));
        }
        catch (const DownloadException& e)
        {
            mpl::log(mpl::Level::error, category, fmt::format("Error fetching workflows: {}", e.what()));
        }
    }
}
