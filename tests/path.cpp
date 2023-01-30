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

#include "path.h"
#include <multipass/format.h>

#include <QCoreApplication>
#include <QDir>

namespace mpt = multipass::test;

QString mpt::test_data_path()
{
    QDir dir{QCoreApplication::applicationDirPath()};
    const auto test_data_dir_exists = dir.cd("test_data");
    if (!test_data_dir_exists)
        throw std::runtime_error("could not find test_data directory");
    return QDir::toNativeSeparators(dir.path()) + QDir::separator();
}

QString mpt::test_data_path_for(const char* file_name)
{
    QDir dir{test_data_path()};
    return dir.filePath(file_name);
}

QString mpt::test_data_sub_dir_path(const char* dir_name)
{
    QDir dir{test_data_path()};
    if (!dir.cd(dir_name))
        throw std::runtime_error(fmt::format("could not find sub dir '{}' under test_data directory", dir_name));
    return QDir::toNativeSeparators(dir.path()) + QDir::separator();
}

std::string mpt::mock_bin_path()
{
    QDir dir{QCoreApplication::applicationDirPath()};
    const auto mocks_dir_exists = dir.cd("mocks");
    if (!mocks_dir_exists)
        throw std::runtime_error("could not find mock binaries directory");
    return QDir::toNativeSeparators(dir.path()).toStdString();
}
