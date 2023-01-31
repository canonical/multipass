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

#include <multipass/cli/alias_dict.h>
#include <multipass/constants.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/json_writer.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTemporaryFile>

namespace mp = multipass;
namespace mpu = multipass::utils;

namespace
{
void check_working_directory_string(const std::string& dir)
{
    if (dir != "map" && dir != "default")
        throw std::runtime_error(fmt::format("invalid working_directory string \"{}\"", dir));
}
} // namespace

mp::AliasDict::AliasDict(mp::Terminal* term) : cout(term->cout()), cerr(term->cerr())
{
    const auto file_name = QStringLiteral("%1_aliases.json").arg(mp::client_name);
    const auto user_config_path = QDir{MP_STDPATHS.writableLocation(mp::StandardPaths::GenericConfigLocation)};
    const auto cli_client_dir_path = QDir{user_config_path.absoluteFilePath(mp::client_name)};

    aliases_file = cli_client_dir_path.absoluteFilePath(file_name).toStdString();

    load_dict();
}

mp::AliasDict::~AliasDict()
{
    if (modified)
    {
        try
        {
            save_dict();
        }
        catch (std::runtime_error& e)
        {
            cerr << fmt::format("Error saving aliases dictionary: {}\n", e.what());
        }
    }
}

void mp::AliasDict::set_active_context(const std::string& new_active_context)
{
    if (new_active_context != active_context)
    {
        modified = true;
        active_context = new_active_context;
    }

    // When switching active context, make sure that a context associated with the new active context exists.
    if (aliases.try_emplace(active_context, mp::AliasContext{}).second)
        modified = true;
}

std::string mp::AliasDict::active_context_name() const
{
    return active_context;
}

const mp::AliasContext& mp::AliasDict::get_active_context() const
{
    try
    {
        return aliases.at(active_context);
    }
    catch (const std::out_of_range&)
    {
        throw std::runtime_error(fmt::format("active context \"{}\" does not exist in dictionary", active_context));
    }
}

bool mp::AliasDict::add_alias(const std::string& alias, const mp::AliasDefinition& command)
{
    if (aliases[active_context].try_emplace(alias, command).second)
    {
        modified = true;

        return true;
    }

    return false;
}

bool mp::AliasDict::exists_alias(const std::string& alias) const
{
    return (aliases.count(active_context) && aliases.at(active_context).count(alias));
}

bool mp::AliasDict::remove_alias(const std::string& alias)
{
    if (aliases[active_context].erase(alias) > 0)
    {
        modified = true;
        return true;
    }

    return false;
}

bool mp::AliasDict::remove_context(const std::string& context)
{
    if (aliases.erase(context) > 0)
    {
        modified = true;

        if (active_context == context)
            set_active_context(default_context_name);

        return true;
    }

    return false;
}

// This function removes the context called as the instance, and then it iterates through all the remaining contexts
// to see if there are aliases defined for the instance.
std::vector<std::pair<std::string, std::string>> mp::AliasDict::remove_aliases_for_instance(const std::string& instance)
{
    std::vector<std::pair<std::string, std::string>> removed_aliases;

    remove_context(instance);

    const auto old_context_name = active_context;

    for (auto dict_it = begin(); dict_it != end(); ++dict_it)
        for (auto context_it = dict_it->second.begin(); context_it != dict_it->second.end(); ++context_it)
            if (context_it->second.instance == instance)
                removed_aliases.emplace_back(std::make_pair(dict_it->first, context_it->first));

    for (auto pair_to_remove : removed_aliases)
    {
        auto [context_from, removed_alias] = pair_to_remove;

        set_active_context(context_from);
        remove_alias(removed_alias);
    }

    active_context = old_context_name;

    return removed_aliases;
}

std::optional<std::pair<std::string, std::string>> mp::AliasDict::get_context_and_alias(const std::string& alias) const
{
    try
    {
        if (aliases.at(active_context).count(alias) > 0)
            return std::make_pair(active_context, alias);
    }
    catch (const std::out_of_range&)
    {
    }

    std::string::size_type dot_pos = alias.rfind('.');

    if (dot_pos == std::string::npos)
        return std::nullopt;

    std::string context = alias.substr(0, dot_pos);

    if (aliases.count(context) == 0)
        return std::nullopt;

    std::string alias_only = alias.substr(dot_pos + 1);

    return (aliases.at(context).count(alias_only) == 0 ? std::nullopt
                                                       : std::make_optional(std::make_pair(context, alias_only)));
}

std::optional<mp::AliasDefinition> mp::AliasDict::get_alias(const std::string& alias) const
{
    std::string::size_type dot_pos;

    try
    {
        return aliases.at(active_context).at(alias);
    }
    catch (const std::out_of_range&)
    {
        if ((dot_pos = alias.find('.')) == std::string::npos)
            return std::nullopt;
    }

    std::string context = alias.substr(0, dot_pos);
    std::string alias_only = alias.substr(dot_pos + 1);

    try
    {
        return aliases.at(context).at(alias_only);
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;
    }
}

QJsonObject mp::AliasDict::to_json() const
{
    auto alias_to_json = [](const mp::AliasDefinition& alias) -> QJsonObject {
        QJsonObject json;

        check_working_directory_string(alias.working_directory);

        json.insert("instance", QString::fromStdString(alias.instance));
        json.insert("command", QString::fromStdString(alias.command));
        json.insert("working-directory", QString::fromStdString(alias.working_directory));

        return json;
    };

    QJsonObject dict_json, all_contexts_json;
    dict_json.insert("active-context", QString::fromStdString(active_context));

    for (const auto& [context_name, context_contents] : aliases)
    {
        QJsonObject context_json;

        for (const auto& [alias_name, alias_definition] : context_contents)
        {
            context_json.insert(QString::fromStdString(alias_name), alias_to_json(alias_definition));
        }

        all_contexts_json.insert(QString::fromStdString(context_name), context_json);
    }

    dict_json.insert(QString("contexts"), all_contexts_json);

    return dict_json;
}

void mp::AliasDict::load_dict()
{
    QFile db_file{QString::fromStdString(aliases_file)};

    aliases.clear();

    if (MP_FILEOPS.exists(db_file))
    {
        if (!MP_FILEOPS.open(db_file, QIODevice::ReadOnly))
            throw std::runtime_error(fmt::format("Error opening file '{}'", db_file.fileName()));
    }
    else
    {
        active_context = default_context_name;
        aliases[default_context_name] = AliasContext();

        return;
    }

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(db_file.readAll(), &parse_error);
    if (doc.isNull())
    {
        active_context = default_context_name;
        aliases[default_context_name] = AliasContext();

        return;
    }

    QJsonObject records = doc.object();
    if (records.isEmpty())
    {
        active_context = default_context_name;
        aliases[default_context_name] = AliasContext();

        return;
    }

    auto records_to_context = [](const QJsonObject& records, AliasContext& context) -> void {
        for (auto it = records.constBegin(); it != records.constEnd(); ++it)
        {
            auto alias = it.key().toStdString();
            auto record = it.value().toObject();
            if (record.isEmpty())
                break;

            auto instance = record["instance"].toString().toStdString();
            auto command = record["command"].toString().toStdString();

            auto read_working_directory = record["working-directory"];
            std::string working_directory;

            if (read_working_directory.isString() && !read_working_directory.toString().isEmpty())
                working_directory = read_working_directory.toString().toStdString();
            else
                working_directory = "default";

            check_working_directory_string(working_directory);

            context.emplace(alias, mp::AliasDefinition{instance, command, working_directory});
        }
    };

    // If the JSON does not contain the active-context field, then the file was written by a version of Multipass
    // previous than the introduction of alias contexts.
    if (records.contains("active-context"))
    {
        active_context = records["active-context"].toString().toStdString();

        auto context_array = records["contexts"].toObject();

        for (auto it = context_array.constBegin(); it != context_array.constEnd(); ++it)
        {
            AliasContext context;
            std::string context_name = it.key().toStdString();
            records_to_context(it.value().toObject(), context);
            aliases.emplace(std::make_pair(context_name, context));
        }
    }
    else
    {
        // The file with the old format does not contain information about contexts. For that reason, everything
        // defined goes into the default context.
        active_context = default_context_name;

        AliasContext default_context;
        records_to_context(records, default_context);
        aliases.emplace(std::make_pair(active_context, default_context));
    }

    db_file.close();
}

void mp::AliasDict::save_dict()
{
    sanitize_contexts();

    QJsonObject dict_json = to_json();

    auto config_file_name = QString::fromStdString(aliases_file);

    QTemporaryFile temp_file{mpu::create_temp_file_with_path(config_file_name)};

    if (MP_FILEOPS.open(temp_file, QIODevice::ReadWrite))
    {
        temp_file.setAutoRemove(false);

        mp::write_json(dict_json, temp_file.fileName());

        temp_file.close();

        if (MP_FILEOPS.exists(QFile{config_file_name}))
        {
            auto backup_file_name = config_file_name + ".bak";
            QFile backup_file(backup_file_name);

            if (MP_FILEOPS.exists(backup_file) && !MP_FILEOPS.remove(backup_file))
                throw std::runtime_error(fmt::format("cannot remove old aliases backup file {}", backup_file_name));

            QFile config_file(config_file_name);
            if (!MP_FILEOPS.rename(config_file, backup_file_name))
                throw std::runtime_error(fmt::format("cannot rename aliases config to {}", backup_file_name));
        }

        if (!MP_FILEOPS.rename(temp_file, config_file_name))
            throw std::runtime_error(fmt::format("cannot create aliases config file {}", config_file_name));
    }
}

// This function removes the contexts which do not contain aliases, except the active context.
void mp::AliasDict::sanitize_contexts()
{
    // To avoid invalidating iterators, the function works in two stages. First, the aliases which need to be
    // removed are determined and, second, they are effectively removed.
    std::vector<std::string> empty_contexts;

    for (auto& context : aliases)
        if (context.first != active_context && context.second.empty())
            empty_contexts.push_back(context.first);

    if (!empty_contexts.empty())
    {
        modified = true;
        for (const auto& context : empty_contexts)
            aliases.erase(context);
    }
}
