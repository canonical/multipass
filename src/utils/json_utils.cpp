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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/json_utils.h>

#include <QJsonDocument>
#include <QSaveFile>

#include <stdexcept>

namespace mp = multipass;

mp::JsonUtils::JsonUtils(const Singleton<JsonUtils>::PrivatePass& pass) noexcept : Singleton<JsonUtils>{pass}
{
}

void mp::JsonUtils::write_json(const QJsonObject& root, QString file_name) const
{
    auto dir = QFileInfo(file_name).absoluteDir();
    if (!MP_FILEOPS.mkpath(dir, "."))
        throw std::runtime_error(fmt::format("Could not create path '{}'", dir.absolutePath()));

    QSaveFile db_file{file_name};
    if (!MP_FILEOPS.open(db_file, QIODevice::WriteOnly))
        throw std::runtime_error{fmt::format("Could not open transactional file for writing; filename: {}", file_name)};

    if (MP_FILEOPS.write(db_file, QJsonDocument{root}.toJson()) == -1)
        throw std::runtime_error{fmt::format("Could not write json to transactional file; filename: {}; error: {}",
                                             file_name,
                                             db_file.errorString())};

    if (!MP_FILEOPS.commit(db_file))
        throw std::runtime_error{fmt::format("Could not commit transactional file; filename: {}", file_name)};
}

std::string mp::JsonUtils::json_to_string(const QJsonObject& root) const
{
    // The function name toJson() is shockingly wrong, for it converts an actual JsonDocument to a QByteArray.
    return QJsonDocument(root).toJson().toStdString();
}
