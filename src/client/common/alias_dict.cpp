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

bool mp::AliasDict::add_alias(const std::string& alias, const mp::AliasDefinition& command)
{
    if (aliases.try_emplace(alias, command).second)
    {
        modified = true;

        return true;
    }

    return false;
}

bool mp::AliasDict::exists_alias(const std::string& alias)
{
    return aliases.count(alias);
}

bool mp::AliasDict::remove_alias(const std::string& alias)
{
    if (aliases.erase(alias) > 0)
    {
        modified = true;
        return true;
    }

    return false;
}

std::vector<std::string> mp::AliasDict::remove_aliases_for_instance(const std::string& instance)
{
    std::vector<std::string> removed_aliases;

    for (auto it = aliases.begin(); it != aliases.end();)
    {
        if (it->second.instance == instance)
        {
            modified = true;
            removed_aliases.push_back(it->first);
            it = aliases.erase(it);
        }
        else
            ++it;
    }

    return removed_aliases;
}

std::optional<mp::AliasDefinition> mp::AliasDict::get_alias(const std::string& alias) const
{
    try
    {
        return aliases.at(alias);
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;
    }
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
        return;

    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(db_file.readAll(), &parse_error);
    if (doc.isNull())
        return;

    QJsonObject records = doc.object();
    if (records.isEmpty())
        return;

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

        aliases.emplace(alias, mp::AliasDefinition{instance, command, working_directory});
    }

    db_file.close();
}

void mp::AliasDict::save_dict()
{
    auto alias_to_json = [](const mp::AliasDefinition& alias) -> QJsonObject {
        QJsonObject json;

        check_working_directory_string(alias.working_directory);

        json.insert("instance", QString::fromStdString(alias.instance));
        json.insert("command", QString::fromStdString(alias.command));
        json.insert("working-directory", QString::fromStdString(alias.working_directory));

        return json;
    };

    QJsonObject aliases_json;
    for (const auto& record : aliases)
    {
        auto name = QString::fromStdString(record.first);
        auto definition = alias_to_json(record.second);

        aliases_json.insert(name, definition);
    }

    auto config_file_name = QString::fromStdString(aliases_file);

    QTemporaryFile temp_file{mpu::create_temp_file_with_path(config_file_name)};

    if (MP_FILEOPS.open(temp_file, QIODevice::ReadWrite))
    {
        temp_file.setAutoRemove(false);

        mp::write_json(aliases_json, temp_file.fileName());

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
