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

#pragma once

#include <QString>
#include <QStringList>

#include <boost/json.hpp>

#include <multipass/exceptions/unsupported_arch_exception.h>
#include <multipass/json_utils.h>

namespace multipass
{
struct VMImageInfo
{
    QStringList aliases;
    QString os;
    QString release;
    QString release_title;
    QString release_codename;
    bool supported;
    QString image_location;
    QString id;
    QString stream_location;
    QString version;
    int64_t size;
    bool verify;

    friend inline bool operator==(const VMImageInfo& a, const VMImageInfo& b) = default;
};

struct for_arch
{
    std::string arch;
};

inline VMImageInfo tag_invoke(const boost::json::value_to_tag<VMImageInfo>&,
                              const boost::json::value& json,
                              const for_arch& arch)
{
    const auto arch_json = json.at("items").try_at(arch.arch);
    if (arch_json.has_error())
        throw UnsupportedArchException(arch.arch);

    QStringList aliases = value_to<QString>(json.at("aliases")).split(",", Qt::SkipEmptyParts);
    for (QString& alias : aliases)
        alias = alias.trimmed();

    return {aliases,
            value_to<QString>(json.at("os")),
            value_to<QString>(json.at("release")),
            value_to<QString>(json.at("release_codename")),
            value_to<QString>(json.at("release_title")),
            true,
            value_to<QString>(arch_json->at("image_location")),
            value_to<QString>(arch_json->at("id")),
            "",
            value_to<QString>(arch_json->at("version")),
            lookup_or<int>(*arch_json, "size", -1),
            true};
}
} // namespace multipass
