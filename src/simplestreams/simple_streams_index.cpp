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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *              Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/simple_streams_index.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;

namespace
{
QJsonObject parse_index(const QByteArray& json)
{
    QJsonParseError parse_error;
    auto doc = QJsonDocument::fromJson(json, &parse_error);
    if (doc.isNull())
        throw std::runtime_error(parse_error.errorString().toStdString());

    if (!doc.isObject())
        throw std::runtime_error("invalid index object");

    auto index = doc.object()["index"].toObject();
    if (index.isEmpty())
        throw std::runtime_error("No index found");

    return index;
}
}

mp::SimpleStreamsIndex mp::SimpleStreamsIndex::fromJson(const QByteArray& json)
{
    auto index = parse_index(json);

    for (QJsonValueRef value : index)
    {
        auto entry = value.toObject();
        if (entry["datatype"] == "image-downloads")
        {
            auto path_entry = entry["path"];
            auto date_entry = entry["updated"];
            return {path_entry.toString(), date_entry.toString()};
        }
    }

    throw std::runtime_error("no image-downloads entry found");
}
