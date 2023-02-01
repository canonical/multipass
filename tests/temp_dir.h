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

#ifndef MULTIPASS_TEMP_DIR_H
#define MULTIPASS_TEMP_DIR_H

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

    QString filePath(const QString& fileName)
    {
        return dir.filePath(fileName);
    }

private:
    QTemporaryDir dir;
    QString the_path;
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_TEMP_DIR_H
