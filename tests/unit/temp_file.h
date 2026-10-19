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

#pragma once

#include <QString>
#include <QTemporaryFile>
#include <QUrl>

#include <filesystem>

namespace multipass
{
namespace test
{
class TempFile
{
public:
    TempFile();
    QString name() const
    {
        return the_name;
    }
    std::filesystem::path path() const
    {
        return the_name.toStdString();
    }
    QString url() const
    {
        return the_url;
    }

private:
    QTemporaryFile file;
    QString the_name;
    QString the_url;
};
} // namespace test
} // namespace multipass
