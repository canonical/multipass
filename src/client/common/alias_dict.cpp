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

#include <multipass/cli/alias_dict.h>
#include <multipass/constants.h>
#include <multipass/json_writer.h>
#include <multipass/standard_paths.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTemporaryFile>

namespace mp = multipass;

mp::AliasDict::AliasDict()
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
        save_dict();
    }
}

void mp::AliasDict::add_alias(const std::string& alias, const mp::AliasDefinition& command)
{
    if (aliases.try_emplace(alias, command).second)
    {
        modified = true;
    }
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

mp::AliasDict::size_type mp::AliasDict::remove_aliases_for_instance(const std::string& instance)
{
    mp::AliasDict::size_type erased_elements = 0;

    for (auto it = aliases.begin(); it != aliases.end();)
    {
        if (it->second.instance == instance)
        {
            ++erased_elements;
            it = aliases.erase(it);
        }
        else
            ++it;
    }

    if (erased_elements > 0)
        modified = true;

    return erased_elements;
}

mp::optional<mp::AliasDefinition> mp::AliasDict::get_alias(const std::string& alias) const
{
    try
    {
        return aliases.at(alias);
    }
    catch (const std::out_of_range&)
    {
        return mp::nullopt;
    }
}

void mp::AliasDict::load_dict()
{
    QFile db_file{QString::fromStdString(aliases_file)};

    aliases.clear();

    if (!db_file.open(QIODevice::ReadOnly))
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

        aliases.emplace(alias, mp::AliasDefinition{instance, command});
    }

    db_file.close();
}

void mp::AliasDict::save_dict()
{
    auto alias_to_json = [](const mp::AliasDefinition& alias) -> QJsonObject {
        QJsonObject json;
        json.insert("instance", QString::fromStdString(alias.instance));
        json.insert("command", QString::fromStdString(alias.command));

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

    auto config_path = QFileInfo(config_file_name).absoluteDir();
    if (!config_path.exists())
    {
        config_path.mkpath(config_path.absolutePath());
    }

    QTemporaryFile temp_file(config_file_name);

    if (temp_file.open())
    {
        temp_file.setAutoRemove(false);

        mp::write_json(aliases_json, temp_file.fileName());

        temp_file.close();

        if (QFile::exists(config_file_name))
        {
            auto backup_file_name = config_file_name + ".bak";
            QFile::remove(backup_file_name);
            QFile::rename(config_file_name, backup_file_name);
        }

        temp_file.rename(config_file_name);
    }
}
