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

#include <multipass/default_vm_blueprint_provider.h>
#include <multipass/exceptions/blueprint_exceptions.h>
#include <multipass/exceptions/download_exception.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
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
const QString github_blueprints_archive_name{"multipass-blueprints.zip"};
const QString blueprint_dir_version{"v1"};
constexpr auto category = "blueprint provider";

auto blueprints_map_for(const std::string& archive_file_path, bool& needs_update)
{
    std::map<std::string, YAML::Node> blueprints_map;
    std::ifstream zip_stream{archive_file_path, std::ios::binary};
    auto zip_archive = MP_POCOZIPUTILS.zip_archive_for(zip_stream);

    for (auto it = zip_archive.headerBegin(); it != zip_archive.headerEnd(); ++it)
    {
        if (it->second.isFile())
        {
            auto file_name = it->second.getFileName();
            QFileInfo file_info{QString::fromStdString(file_name)};

            if (file_info.dir().dirName() == blueprint_dir_version &&
                (file_info.suffix() == "yaml" || file_info.suffix() == "yml"))
            {
                if (!mp::utils::valid_hostname(file_info.baseName().toStdString()))
                {
                    mpl::log(
                        mpl::Level::error, category,
                        fmt::format("Invalid Blueprint name \'{}\': must be a valid host name", file_info.baseName()));
                    needs_update = true;

                    continue;
                }

                Poco::Zip::ZipInputStream zip_input_stream{zip_stream, it->second};
                std::ostringstream out(std::ios::binary);
                Poco::StreamCopier::copyStream(zip_input_stream, out);
                blueprints_map[file_info.baseName().toStdString()] = YAML::Load(out.str());
            }
        }
    }

    return blueprints_map;
}
} // namespace

mp::DefaultVMBlueprintProvider::DefaultVMBlueprintProvider(const QUrl& blueprints_url, URLDownloader* downloader,
                                                           const QDir& archive_dir,
                                                           const std::chrono::milliseconds& blueprints_ttl,
                                                           const QString& arch)
    : blueprints_url{blueprints_url},
      url_downloader{downloader},
      archive_file_path{archive_dir.filePath(github_blueprints_archive_name)},
      blueprints_ttl{blueprints_ttl},
      arch{arch}
{
    update_blueprints();
}

mp::DefaultVMBlueprintProvider::DefaultVMBlueprintProvider(URLDownloader* downloader, const QDir& archive_dir,
                                                           const std::chrono::milliseconds& blueprints_ttl,
                                                           const QString& arch)
    : DefaultVMBlueprintProvider(default_blueprint_url, downloader, archive_dir, blueprints_ttl, arch)
{
}

mp::Query mp::DefaultVMBlueprintProvider::fetch_blueprint_for(const std::string& blueprint_name,
                                                              VirtualMachineDescription& vm_desc,
                                                              ClientLaunchData& client_launch_data)
{
    update_blueprints();

    Query query{"", "default", false, "", Query::Type::Alias};
    auto& blueprint_config = blueprint_map.at(blueprint_name);

    if (!blueprint_config["instances"][blueprint_name])
    {
        throw InvalidBlueprintException(
            fmt::format("There are no instance definitions matching Blueprint name \"{}\"", blueprint_name));
    }

    auto blueprint_aliases = blueprint_config["aliases"];
    if (blueprint_aliases)
    {
        for (const auto& alias_to_be_defined : blueprint_aliases)
        {
            auto alias_name = alias_to_be_defined.first.as<std::string>();
            auto instance_and_command = mp::utils::split(alias_to_be_defined.second.as<std::string>(), ":");
            if (instance_and_command.size() != 2)
                throw InvalidBlueprintException(fmt::format("Alias definition must be in the form instance:command"));

            mpl::log(mpl::Level::trace, category,
                     fmt::format("Add alias [{}, {}, {}] to RPC answer", alias_name, instance_and_command[0],
                                 instance_and_command[1]));
            AliasDefinition alias_definition{instance_and_command[0], instance_and_command[1], "map"};
            client_launch_data.aliases_to_be_created.emplace(alias_name, alias_definition);
        }
    }

    auto blueprint_instance = blueprint_config["instances"][blueprint_name];

    // TODO: Abstract all of the following YAML schema boilerplate
    if (blueprint_instance["image"])
    {
        // TODO: Support http later.
        // This only supports the "alias" and "remote:alias" scheme at this time
        auto image_str{blueprint_config["instances"][blueprint_name]["image"].as<std::string>()};
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
            throw InvalidBlueprintException("Unsupported image scheme in Blueprint");
        }
    }

    if (blueprint_instance["limits"]["min-cpu"])
    {
        try
        {
            auto min_cpus = blueprint_instance["limits"]["min-cpu"].as<int>();

            if (vm_desc.num_cores == 0)
            {
                vm_desc.num_cores = min_cpus;
            }
            else if (vm_desc.num_cores < min_cpus)
            {
                throw BlueprintMinimumException("Number of CPUs", std::to_string(min_cpus));
            }
        }
        catch (const YAML::BadConversion&)
        {
            needs_update = true;
            throw InvalidBlueprintException(fmt::format("Minimum CPU value in Blueprint is invalid"));
        }
    }

    if (blueprint_instance["limits"]["min-mem"])
    {
        auto min_mem_size_str{blueprint_instance["limits"]["min-mem"].as<std::string>()};

        try
        {
            MemorySize min_mem_size{min_mem_size_str};

            if (vm_desc.mem_size.in_bytes() == 0)
            {
                vm_desc.mem_size = min_mem_size;
            }
            else if (vm_desc.mem_size < min_mem_size)
            {
                throw BlueprintMinimumException("Memory size", min_mem_size_str);
            }
        }
        catch (const InvalidMemorySizeException&)
        {
            needs_update = true;
            throw InvalidBlueprintException(fmt::format("Minimum memory size value in Blueprint is invalid"));
        }
    }

    if (blueprint_instance["limits"]["min-disk"])
    {
        auto min_disk_space_str{blueprint_instance["limits"]["min-disk"].as<std::string>()};

        try
        {
            MemorySize min_disk_space{min_disk_space_str};

            if (vm_desc.disk_space.in_bytes() == 0)
            {
                vm_desc.disk_space = min_disk_space;
            }
            else if (vm_desc.disk_space < min_disk_space)
            {
                throw BlueprintMinimumException("Disk space", min_disk_space_str);
            }
        }
        catch (const InvalidMemorySizeException&)
        {
            needs_update = true;
            throw InvalidBlueprintException(fmt::format("Minimum disk space value in Blueprint is invalid"));
        }
    }

    if (blueprint_instance["cloud-init"]["vendor-data"])
    {
        try
        {
            auto cloud_init_config = YAML::Load(blueprint_instance["cloud-init"]["vendor-data"].as<std::string>());

            for (const auto& config : cloud_init_config)
            {
                if (config.first.IsScalar())
                {
                    vm_desc.vendor_data_config[config.first.Scalar()] = config.second;
                }
            }
        }
        catch (const YAML::BadConversion&)
        {
            needs_update = true;
            throw InvalidBlueprintException(
                fmt::format("Cannot convert cloud-init data for the {} Blueprint", blueprint_name));
        }
    }

    auto blueprint_workspaces = blueprint_instance["workspace"];
    if (blueprint_workspaces && blueprint_workspaces.as<bool>())
    {
        mpl::log(mpl::Level::trace, category, fmt::format("Add workspace {} to RPC answer", blueprint_name));
        client_launch_data.workspaces_to_be_created.push_back(blueprint_name);
    }

    return query;
}

mp::VMImageInfo mp::DefaultVMBlueprintProvider::info_for(const std::string& blueprint_name)
{
    update_blueprints();

    static constexpr auto missing_key_template{"The \'{}\' key is required for the {} Blueprint"};
    static constexpr auto bad_conversion_template{"Cannot convert \'{}\' key for the {} Blueprint"};
    auto& blueprint_config = blueprint_map.at(blueprint_name);

    VMImageInfo image_info;
    image_info.aliases.append(QString::fromStdString(blueprint_name));

    const auto description_key{"description"};
    const auto version_key{"version"};
    const auto runs_on_key{"runs-on"};

    if (blueprint_config[runs_on_key])
    {
        try
        {
            auto runs_on = blueprint_config[runs_on_key].as<std::vector<std::string>>();
            if (std::find(runs_on.cbegin(), runs_on.cend(), arch.toStdString()) == runs_on.cend())
            {
                throw IncompatibleBlueprintException(blueprint_name);
            }
        }
        catch (const YAML::BadConversion&)
        {
            throw InvalidBlueprintException(fmt::format(bad_conversion_template, runs_on_key, blueprint_name));
        }
    }

    if (!blueprint_config[description_key])
    {
        needs_update = true;
        throw InvalidBlueprintException(fmt::format(missing_key_template, description_key, blueprint_name));
    }

    if (!blueprint_config[version_key])
    {
        needs_update = true;
        throw InvalidBlueprintException(fmt::format(missing_key_template, version_key, blueprint_name));
    }

    try
    {
        image_info.release_title = QString::fromStdString(blueprint_config[description_key].as<std::string>());
    }
    catch (const YAML::BadConversion&)
    {
        needs_update = true;
        throw InvalidBlueprintException(fmt::format(bad_conversion_template, description_key, blueprint_name));
    }

    try
    {
        image_info.version = QString::fromStdString(blueprint_config["version"].as<std::string>());
    }
    catch (const YAML::BadConversion&)
    {
        needs_update = true;
        throw InvalidBlueprintException(fmt::format(bad_conversion_template, version_key, blueprint_name));
    }

    return image_info;
}

std::vector<mp::VMImageInfo> mp::DefaultVMBlueprintProvider::all_blueprints()
{
    update_blueprints();

    bool will_need_update{false};
    std::vector<VMImageInfo> blueprint_info;

    for (const auto& [key, config] : blueprint_map)
    {
        try
        {
            blueprint_info.push_back(info_for(key));
        }
        catch (const InvalidBlueprintException& e)
        {
            // Don't force updates in info_for() since we are looping and only force the update once we
            // finish iterating.
            needs_update = false;
            will_need_update = true;
            mpl::log(mpl::Level::error, category, fmt::format("Invalid Blueprint: {}", e.what()));
        }
        catch (const IncompatibleBlueprintException& e)
        {
            mpl::log(mpl::Level::trace, category, fmt::format("Skipping incompatible Blueprint: {}", e.what()));
        }
    }

    if (will_need_update)
        needs_update = true;

    return blueprint_info;
}

std::string mp::DefaultVMBlueprintProvider::name_from_blueprint(const std::string& blueprint_name)
{
    if (blueprint_map.count(blueprint_name) == 1)
        return blueprint_name;

    return {};
}

int mp::DefaultVMBlueprintProvider::blueprint_timeout(const std::string& blueprint_name)
{
    auto timeout_seconds{0};

    try
    {
        blueprint_map.at(blueprint_name);

        auto& blueprint_config = blueprint_map.at(blueprint_name);

        auto blueprint_instance = blueprint_config["instances"][blueprint_name];

        if (blueprint_instance["timeout"])
        {
            try
            {
                timeout_seconds = blueprint_instance["timeout"].as<int>();
            }
            catch (const YAML::BadConversion&)
            {
                needs_update = true;
                throw InvalidBlueprintException(fmt::format("Invalid timeout given in Blueprint"));
            }
        }
    }
    catch (const std::out_of_range&)
    {
        // Ignore
    }

    return timeout_seconds;
}

void mp::DefaultVMBlueprintProvider::fetch_blueprints()
{
    url_downloader->download_to(blueprints_url, archive_file_path, -1, -1, [](auto...) { return true; });

    blueprint_map = blueprints_map_for(archive_file_path.toStdString(), needs_update);
}

void mp::DefaultVMBlueprintProvider::update_blueprints()
{
    const auto now = std::chrono::steady_clock::now();
    if ((now - last_update) > blueprints_ttl || needs_update)
    {
        try
        {
            fetch_blueprints();
            last_update = now;
            needs_update = false;
        }
        catch (const Poco::Exception& e)
        {
            mpl::log(mpl::Level::error, category,
                     fmt::format("Error extracting Blueprints zip file: {}", e.displayText()));
        }
        catch (const DownloadException& e)
        {
            mpl::log(mpl::Level::error, category, fmt::format("Error fetching Blueprints: {}", e.what()));
        }
    }
}
