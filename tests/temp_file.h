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

#ifndef MULTIPASS_TEMP_FILE_H
#define MULTIPASS_TEMP_FILE_H

#include <QString>
#include <QTemporaryFile>
#include <QUrl>

namespace multipass
{
namespace test
{
class TempFile
{
public:
    TempFile();
    QString name() const { return the_name; }
    QString url() const { return the_url; }
private:
    QTemporaryFile file;
    QString the_name;
    QString the_url;
};
}
}

#endif // MULTIPASS_TEMP_FILE_H
