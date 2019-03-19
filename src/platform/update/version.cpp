/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "version.h"

#include <QRegExp>
#include <QStringList>
#include <fmt/format.h>
#include <multipass/logging/log.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
QString extract_tag_from_git_describe_output(const QString& version)
{
    auto tokens = version.split('-');
    if (tokens.count() <= 2)
    { // whole version string is the tag
        return version;
    }
    else
    { // likely git has appended number and shortened SHA string, drop them
        tokens.removeLast();
        tokens.removeLast();
        return tokens.join('-');
    }
}
} // namespace

mp::Version::Version(const QString& version_string)
{
    auto tag = extract_tag_from_git_describe_output(version_string);
    if (tag.isEmpty())
        throw std::runtime_error(fmt::format("Failed to parse version string: '{}", qPrintable(version_string)));

    QRegExp regex("^v(\\d+)\\.(\\d+)(?:-(.*))?$");
    if (!regex.exactMatch(tag))
        throw std::runtime_error(fmt::format("Version tag of unknown format: '{}'", qPrintable(tag)));

    auto captures = regex.capturedTexts();
    ver_major = captures[1].toInt();
    ver_minor = captures[2].toInt();
    if (captures.count() > 3)
        ver_modifier = captures[3];
}

bool mp::Version::operator<(const Version& other)
{
    if (ver_major == other.ver_major && ver_minor == other.ver_minor)
    {
        // TODO: deep comparison of "preX" numbering is not being considered
        if (ver_modifier == other.ver_modifier || other.ver_modifier.contains("pre"))
            return false;
        if (ver_modifier.contains("pre"))
            return true;
    }

    if (ver_major < other.ver_major)
        return true;

    if (ver_minor < other.ver_minor)
        return true;

    return false;
}

int mp::Version::major() const
{
    return ver_major;
}

int mp::Version::minor() const
{
    return ver_minor;
}

QString mp::Version::modifier() const
{
    return ver_modifier;
}
