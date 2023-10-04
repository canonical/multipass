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

#ifndef MULTIPASS_JSON_UTILS_H
#define MULTIPASS_JSON_UTILS_H

#include <string>

#include <QJsonObject>
#include <QString>

namespace multipass
{
void write_json(const QJsonObject& root, QString file_name); // transactional; requires the parent directory to exist
std::string json_to_string(const QJsonObject& root);
}
#endif // MULTIPASS_JSON_UTILS_H
