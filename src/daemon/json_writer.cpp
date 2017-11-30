/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include "json_writer.h"

#include <QFile>
#include <QJsonDocument>

namespace mp = multipass;

void mp::write_json(const QJsonObject& root, QString file_name)
{
    QJsonDocument doc{root};
    auto raw_json = doc.toJson();
    QFile db_file{file_name};
    db_file.open(QIODevice::ReadWrite | QIODevice::Truncate);
    db_file.write(raw_json);
}
