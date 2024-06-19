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

#include "file_operations.h"
#include "path.h"

#include <multipass/format.h>
#include <multipass/utils.h>

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

void mpt::make_file_with_content(const QString& file_name, const std::string& content)
{
    MP_UTILS.Utils::make_file_with_content(file_name.toStdString(), content); // call the base impl even if it is a mock
}
