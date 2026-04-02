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

#include <multipass/platform.h>

#include <filesystem>

#include <QString>
#include <QTemporaryDir>

namespace multipass
{
namespace test
{
class TempDir
{
public:
    TempDir();
    QString path() const
    {
        return the_path;
    }

    operator std::filesystem::path() const
    {
        return MP_PLATFORM.qstr_to_path(the_path);
    }

    QString filePath(const QString& fileName)
    {
        return dir.filePath(fileName);
    }

    std::filesystem::path operator/(const std::string& fileName)
    {
        return static_cast<std::filesystem::path>(*this) / fileName;
    }

private:
    QTemporaryDir dir;
    QString the_path;
};
} // namespace test
} // namespace multipass
