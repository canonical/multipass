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
#include <multipass/platform.h>
#include <multipass/poco_zip_utils.h>
#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/utils.h>
#include <multipass/yaml_node_utils.h>

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

// The folders where to read definitions. The list is sorted by precedence.
constexpr auto version_v1{"v1"};
constexpr auto version_v2{"v2"};
const QStringList blueprint_dir_versions{version_v2, version_v1};

constexpr auto category = "blueprint provider";

static constexpr auto bad_conversion_template{"Cannot convert \'{}\' key for the {} Blueprint"};
const auto runs_on_key{"runs-on"};
const auto instances_key{"instances"};

bool runs_on(const std::string& blueprint_name, const YAML::Node& blueprint_node, const std::string& arch)
{
    if (blueprint_node["blueprint-version"].as<std::string>() == version_v1)
    {
        if (blueprint_node[runs_on_key])
        {
            try
            {
                const auto runs_on = blueprint_node[runs_on_key].as<std::vector<std::string>>();

                return std::find(runs_on.cbegin(), runs_on.cend(), arch) != runs_on.cend();
            }
            catch (const YAML::BadConversion&)
            {
                throw mp::InvalidBlueprintException(fmt::format(bad_conversion_template, runs_on_key, blueprint_name));
            }
        }
        else
        {
            return true;
        }
    }

    if (blueprint_node[instances_key] && blueprint_node[instances_key][blueprint_name] &&
        blueprint_node[instances_key][blueprint_name]["images"])
    {
        if (blueprint_node[instances_key][blueprint_name]["images"][arch])
        {
            if (blueprint_node[instances_key][blueprint_name]["images"][arch]["url"])
                return true;
        }
        else
        {
            return false;
        }
    }

    throw mp::InvalidBlueprintException(fmt::format(bad_conversion_template, instances_key, blueprint_name));
}

auto blueprints_map_for(const std::string& archive_file_path, bool& needs_update, const std::string& arch)
{
    std::map<std::string, YAML::Node> blueprints_map;
    std::ifstream zip_stream{archive_file_path, std::ios::binary};
    auto zip_archive = MP_POCOZIPUTILS.zip_archive_for(zip_stream);

    for (const auto& blueprint_dir_version : blueprint_dir_versions)
    {
        for (auto it = zip_archive.headerBegin(); it != zip_archive.headerEnd(); ++it)
        {
            if (it->second.isFile())
            {
                auto file_name = it->second.getFileName();
                QFileInfo file_info{QString::fromStdString(file_name)};
                auto blueprint_name = file_info.baseName().toStdString();

                if (!blueprints_map.count(blueprint_name) && file_info.dir().dirName() == blueprint_dir_version &&
                    (file_info.suffix() == "yaml" || file_info.suffix() == "yml"))
                {
                    if (!mp::utils::valid_hostname(blueprint_name))
                    {
                        mpl::log(mpl::Level::error, category,
                                 fmt::format("Invalid Blueprint name \'{}\': must be a valid host name",
                                             file_info.baseName()));
                        needs_update = true;

                        continue;
                    }

                    Poco::Zip::ZipInputStream zip_input_stream{zip_stream, it->second};
                    std::ostringstream out(std::ios::binary);
                    Poco::StreamCopier::copyStream(zip_input_stream, out);
                    auto blueprint_node = YAML::Load(out.str());
                    blueprint_node["blueprint-version"] = blueprint_dir_version.toStdString();

                    try
                    {
                        if (runs_on(blueprint_name, blueprint_node, arch))
                        {
                            mpl::log(
                                mpl::Level::debug, category,
                                fmt::format("Loading \"{}\" {}", blueprint_name, blueprint_dir_version.toStdString()));

                            blueprints_map[blueprint_name] = blueprint_node;
                        }
                        else
                        {
                            mpl::log(mpl::Level::debug, category,
                                     fmt::format("Not loading foreign arch \"{}\" {}", blueprint_name,
                                                 blueprint_dir_version.toStdString()));
                        }
                    }
                    catch (mp::InvalidBlueprintException&)
                    {
                        mpl::log(mpl::Level::debug, category,
                                 fmt::format("Not loading malformed \"{}\" {}", blueprint_name,
                                             blueprint_dir_version.toStdString()));
                    }
                }
            }
        }
    }

    return blueprints_map;
}

std::string get_blueprint_version(const YAML::Node& blueprint_instance)
{
    if (blueprint_instance["images"])
        return "v2";

    return "v1";
}

void merge_yaml_entries(YAML::Node& ours, const YAML::Node& theirs, const std::string& key, bool override_null = false)
{
    if (!ours.IsDefined() || (override_null && ours.IsNull()))
    {
        ours = YAML::Clone(theirs);
        return;
    }

    if (ours.IsSequence() && theirs.IsSequence())
    {
        for (const auto& value : theirs)
            ours.push_back(value);

        return;
    }

    if (ours.IsMap() && theirs.IsMap())
    {
        for (const auto& value : theirs)
            if (value.first.IsScalar())
            {
                const auto subkey = value.first.Scalar();
                auto subnode = ours[subkey];
                merge_yaml_entries(subnode, value.second, subkey);
            }

        return;
    }

    throw mp::InvalidBlueprintException{fmt::format("Cannot merge values of {}:\n{}\n\n{}",
                                                    key,
                                                    mp::utils::emit_yaml(ours),
                                                    mp::utils::emit_yaml(theirs))};
}

mp::Query blueprint_from_yaml_node(YAML::Node& blueprint_config, const std::string& blueprint_name,
                                   mp::VirtualMachineDescription& vm_desc, mp::ClientLaunchData& client_launch_data,
                                   const QString& arch, mp::URLDownloader* url_downloader, bool& needs_update)
{
    mp::Query query{"", "default", false, "", mp::Query::Type::Alias};

    if (!blueprint_config[instances_key][blueprint_name])
    {
        throw mp::InvalidBlueprintException(
            fmt::format("There are no instance definitions matching Blueprint name \"{}\"", blueprint_name));
    }

    auto blueprint_instance = blueprint_config[instances_key][blueprint_name];

    if (!blueprint_config["blueprint-version"])
    {
        blueprint_config["blueprint-version"] = get_blueprint_version(blueprint_instance);
    }

    mpl::log(mpl::Level::debug, category,
             fmt::format("Loading Blueprint \"{}\", version {}", blueprint_name,
                         blueprint_config["blueprint-version"].as<std::string>()));

    auto blueprint_aliases = blueprint_config["aliases"];
    if (blueprint_aliases)
    {
        for (const auto& alias_to_be_defined : blueprint_aliases)
        {
            auto alias_name = alias_to_be_defined.first.as<std::string>();
            auto instance_and_command = mp::utils::split(alias_to_be_defined.second.as<std::string>(), ":");
            if (instance_and_command.size() != 2)
                throw mp::InvalidBlueprintException(
                    fmt::format("Alias definition must be in the form instance:command"));

            mpl::log(mpl::Level::trace, category,
                     fmt::format("Add alias [{}, {}, {}] to RPC answer", alias_name, instance_and_command[0],
                                 instance_and_command[1]));
            mp::AliasDefinition alias_definition{instance_and_command[0], instance_and_command[1], "map"};
            client_launch_data.aliases_to_be_created.emplace(alias_name, alias_definition);
        }
    }

    // TODO: Abstract all of the following YAML schema boilerplate

    if (blueprint_config["blueprint-version"].as<std::string>() == version_v2)
    {
        auto arch_node = blueprint_instance["images"][arch.toStdString()];

        if (arch_node["url"])
            query.release = arch_node["url"].as<std::string>();
        else
            throw mp::InvalidBlueprintException(fmt::format("No image URL for architecture {} in Blueprint", arch));

        query.query_type = mp::Query::Type::HttpDownload;

        if (arch_node["sha256"])
        {
            auto sha256_string = arch_node["sha256"].as<std::string>();
            if (QString::fromStdString(sha256_string).startsWith("http"))
            {
                mpl::log(mpl::Level::debug, category, fmt::format("Downloading SHA256 from {}", sha256_string));
                auto downloaded_sha256 = url_downloader->download(QUrl(QString::fromStdString(sha256_string)));
                if (downloaded_sha256.size() > 64)
                    downloaded_sha256.truncate(64); // To account for newlines or other content.
                sha256_string = QString(downloaded_sha256).toStdString();
            }
            mpl::log(mpl::Level::debug, category, fmt::format("Add SHA256 \"{}\" to image record", sha256_string));
            vm_desc.image.id = sha256_string;
        }
        else
        {
            mpl::log(mpl::Level::debug, category, "No SHA256 to check");
        }
    }
    else if (blueprint_instance["image"])
    {
        // This only supports the "alias" and "remote:alias" scheme at this time
        auto image_str{blueprint_config[instances_key][blueprint_name]["image"].as<std::string>()};
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
            throw mp::InvalidBlueprintException("Unsupported image scheme in Blueprint");
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
                throw mp::BlueprintMinimumException("Number of CPUs", std::to_string(min_cpus));
            }
        }
        catch (const YAML::BadConversion&)
        {
            needs_update = true;
            throw mp::InvalidBlueprintException(fmt::format("Minimum CPU value in Blueprint is invalid"));
        }
    }

    if (blueprint_instance["limits"]["min-mem"])
    {
        auto min_mem_size_str{blueprint_instance["limits"]["min-mem"].as<std::string>()};

        try
        {
            mp::MemorySize min_mem_size{min_mem_size_str};

            if (vm_desc.mem_size.in_bytes() == 0)
            {
                vm_desc.mem_size = min_mem_size;
            }
            else if (vm_desc.mem_size < min_mem_size)
            {
                throw mp::BlueprintMinimumException("Memory size", min_mem_size_str);
            }
        }
        catch (const mp::InvalidMemorySizeException&)
        {
            needs_update = true;
            throw mp::InvalidBlueprintException(fmt::format("Minimum memory size value in Blueprint is invalid"));
        }
    }

    if (blueprint_instance["limits"]["min-disk"])
    {
        auto min_disk_space_str{blueprint_instance["limits"]["min-disk"].as<std::string>()};

        try
        {
            mp::MemorySize min_disk_space{min_disk_space_str};

            if (vm_desc.disk_space.in_bytes() == 0)
            {
                vm_desc.disk_space = min_disk_space;
            }
            else if (vm_desc.disk_space < min_disk_space)
            {
                throw mp::BlueprintMinimumException("Disk space", min_disk_space_str);
            }
        }
        catch (const mp::InvalidMemorySizeException&)
        {
            needs_update = true;
            throw mp::InvalidBlueprintException(fmt::format("Minimum disk space value in Blueprint is invalid"));
        }
    }

    if (blueprint_instance["cloud-init"]["vendor-data"])
    {
        try
        {
            auto cloud_init_config = YAML::Load(blueprint_instance["cloud-init"]["vendor-data"].as<std::string>());
            merge_yaml_entries(vm_desc.vendor_data_config, cloud_init_config, "vendor-data", true);
        }
        catch (const YAML::BadConversion&)
        {
            needs_update = true;
            throw mp::InvalidBlueprintException(
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

    auto& blueprint_config = blueprint_map.at(blueprint_name);

    return blueprint_from_yaml_node(blueprint_config, blueprint_name, vm_desc, client_launch_data, arch, url_downloader,
                                    needs_update);
}

mp::Query mp::DefaultVMBlueprintProvider::blueprint_from_file(const std::string& path,
                                                              const std::string& blueprint_name,
                                                              VirtualMachineDescription& vm_desc,
                                                              ClientLaunchData& client_launch_data)
{
    if (!MP_PLATFORM.is_image_url_supported())
        throw std::runtime_error(fmt::format("Launching a Blueprint from a file is not supported"));

    mpl::log(mpl::Level::debug, category, fmt::format("Reading Blueprint '{}' from file {}", blueprint_name, path));

    QFileInfo file_info{QString::fromStdString(path)};

    if (!mp::utils::valid_hostname(blueprint_name))
    {
        auto error_message =
            fmt::format("Invalid Blueprint name \'{}\': must be a valid host name", file_info.baseName());

        mpl::log(mpl::Level::error, category, error_message);

        throw InvalidBlueprintException(error_message);
    }

    YAML::Node blueprint_config;

    try
    {
        blueprint_config = YAML::LoadFile(path);
    }
    catch (const YAML::BadFile&)
    {
        throw InvalidBlueprintException(fmt::format("Wrong file \'{}\'", path));
    }

    return blueprint_from_yaml_node(blueprint_config, blueprint_name, vm_desc, client_launch_data, arch, url_downloader,
                                    needs_update);
}

std::optional<mp::VMImageInfo> mp::DefaultVMBlueprintProvider::info_for(const std::string& blueprint_name)
{
    update_blueprints();

    static constexpr auto missing_key_template{"The \'{}\' key is required for the {} Blueprint"};

    YAML::Node blueprint_config;
    if (auto it = blueprint_map.find(blueprint_name); it != blueprint_map.end())
        blueprint_config = it->second;
    else
        return std::nullopt;

    VMImageInfo image_info;
    image_info.aliases.append(QString::fromStdString(blueprint_name));

    const auto description_key{"description"};
    const auto version_key{"version"};

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
            auto info = info_for(key);
            assert(info && "key missing from blueprint map");
            blueprint_info.push_back(*info);
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

    auto name_qstr = QString::fromStdString(blueprint_name);

    if (name_qstr.startsWith("file://") &&
        (name_qstr.toLower().endsWith(".yaml") || name_qstr.toLower().endsWith(".yml")))
    {
        auto file_path = name_qstr.remove(0, 7);
        auto chop = file_path.at(file_path.size() - 4) == '.' ? 4 : 5;
        return QFileInfo(file_path).fileName().chopped(chop).toStdString();
    }

    return {};
}

int mp::DefaultVMBlueprintProvider::blueprint_timeout(const std::string& blueprint_name)
{
    auto timeout_seconds{0};

    try
    {
        blueprint_map.at(blueprint_name);

        auto& blueprint_config = blueprint_map.at(blueprint_name);

        auto blueprint_instance = blueprint_config[instances_key][blueprint_name];

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

    blueprint_map = blueprints_map_for(archive_file_path.toStdString(), needs_update, arch.toStdString());
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
