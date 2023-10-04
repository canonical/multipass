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
#include <multipass/json_utils.h>

#include <QJsonDocument>
#include <QSaveFile>

namespace mp = multipass;

void mp::write_json(const QJsonObject& root, QString file_name)
{
    QJsonDocument doc{root};
    auto raw_json = doc.toJson();
    QSaveFile db_file{file_name};
    MP_FILEOPS.open(db_file, QIODevice::WriteOnly);
    MP_FILEOPS.write(db_file, raw_json);
    db_file.commit();
}

std::string mp::json_to_string(const QJsonObject& root)
{
    // The function name toJson() is shockingly wrong, for it converts an actual JsonDocument to a QByteArray.
    return QJsonDocument(root).toJson().toStdString();
}
