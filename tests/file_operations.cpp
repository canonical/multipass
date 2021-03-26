/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include "file_operations.h"
#include "path.h"

#include <multipass/format.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace mpt = multipass::test;

QByteArray mpt::load(QString path)
{
    QFile file(path);
    if (file.exists())
    {
        file.open(QIODevice::ReadOnly);
        return file.readAll();
    }
    throw std::invalid_argument(path.toStdString() + " does not exist");
}

QByteArray mpt::load_test_file(const char* file_name)
{
    auto file_path = multipass::test::test_data_path_for(file_name);
    return multipass::test::load(file_path);
}

qint64 mpt::make_file_with_content(const QString& file_name, const std::string& content)
{
    QFile file(file_name);
    if (file.exists())
        throw std::runtime_error(fmt::format("test file already exists: '{}'", file_name));

    QDir parent_dir{QFileInfo{file}.absoluteDir()};
    if (!parent_dir.mkpath(".")) // true if directory already exists
        throw std::runtime_error(fmt::format("failed to create test dir: '{}'", parent_dir.path()));

    QDir file_dir = QFileInfo(file).absoluteDir();
    if (!file_dir.exists())
        file_dir.mkpath(file_dir.absolutePath());

    if (!file.open(QFile::WriteOnly))
        throw std::runtime_error(fmt::format("failed to open test file: '{}'", file_name));

    file.write(content.data(), content.size());
    return file.size();
}
